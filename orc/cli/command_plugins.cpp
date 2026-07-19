/*
 * File:        command_plugins.cpp
 * Module:      orc-cli
 * Purpose:     Plugin registry management subcommand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "command_plugins.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "markdown_examples.h"
#include "project_presenter.h"
#include "stage_category.h"

namespace orc {
namespace cli {

namespace {

void print_plugins_usage(const char* program_name) {
  std::cerr << "Usage: " << program_name << " plugins <subcommand> [options]\n";
  std::cerr << "\n";
  std::cerr << "Subcommands:\n";
  std::cerr << "  list                           Show registry entries and "
               "loaded plugins\n";
  std::cerr << "  stages [--full]                List all available stages "
               "(core + loaded plugins), by role\n";
  std::cerr << "  inputs [--full]                List only input (source) "
               "stages\n";
  std::cerr << "  outputs [--full]               List only output (sink) "
               "stages\n";
  std::cerr << "  filters [--full]               List only processing "
               "stages\n";
  std::cerr << "                                 (--full shows every "
               "parameter inline, like 'describe')\n";
  std::cerr << "  describe <stage name>          Show one stage's parameters "
               "in detail\n";
  std::cerr << "  add <path> [options]           Add a plugin to the "
               "persistent registry\n";
  std::cerr << "  remove <id>                    Remove a plugin from the "
               "persistent registry\n";
  std::cerr << "  enable <id>                    Enable a registered plugin\n";
  std::cerr << "  disable <id>                   Disable a registered plugin\n";
  std::cerr << "  trust <id>                     Mark a registered plugin as "
               "trusted (allows download and loading)\n";
  std::cerr << "  untrust <id>                   Mark a registered plugin as "
               "untrusted (blocks download and loading)\n";
  std::cerr << "  search <term>                  Search the curated plugin "
               "index\n";
  std::cerr << "  info <id>                      Show details for an indexed "
               "plugin\n";
  std::cerr << "  install <id>                   Install a plugin from the "
               "curated index (untrusted until confirmed)\n";
  std::cerr << "\n";
  std::cerr << "Options for 'add':\n";
  std::cerr << "  --id ID                        Plugin identifier (e.g. "
               "com.example.myplugin)\n";
  std::cerr << "  --version VER                  Plugin version string\n";
  std::cerr << "  --license SPDX                 License identifier (e.g. MIT, "
               "GPL-3.0-or-later)\n";
  std::cerr << "  --trusted                      Mark plugin as trusted\n";
  std::cerr << "\n";
  std::cerr
      << "Note: Registry changes take effect on the next application launch.\n";
}

int cmd_plugins_list() {
  const auto registry = orc::presenters::ProjectPresenter::readPluginRegistry();
  const auto loaded = orc::presenters::ProjectPresenter::getLoadedPlugins();

  std::cout << "Registry path: "
            << (registry.registry_path.empty() ? "<none>"
                                               : registry.registry_path)
            << "\n\n";

  if (registry.entries.empty()) {
    std::cout << "No plugins registered.\n";
  } else {
    std::cout << "Registered plugins (" << registry.entries.size() << "):\n";
    for (const auto& e : registry.entries) {
      const std::string id = e.plugin_id.empty() ? "<unnamed>" : e.plugin_id;
      const std::string version =
          e.plugin_version.empty() ? "-" : e.plugin_version;
      const std::string license = e.license_spdx.empty() ? "-" : e.license_spdx;

      std::cout << "  id:       " << id << "\n";
      std::cout << "  path:     " << e.path << "\n";
      std::cout << "  version:  " << version << "\n";
      std::cout << "  license:  " << license << "\n";
      std::cout << "  enabled:  " << (e.enabled ? "yes" : "no") << "\n";
      std::cout << "  trusted:  "
                << ((e.is_core_plugin || e.trust_state == "trusted") ? "yes"
                                                                     : "no")
                << "\n";
      std::cout << "  core:     " << (e.is_core_plugin ? "yes" : "no") << "\n";
      std::cout << "  exists:   " << (e.path_exists ? "yes" : "no") << "\n";
      std::cout << "  loaded:   " << (e.is_loaded ? "yes" : "no") << "\n";
      if (e.required_host_abi != 0) {
        std::cout << "  host ABI: requires " << e.required_host_abi << " (host "
                  << e.host_abi_version << ")";
        if (!e.abi_compatible) {
          std::cout << " — needs rebuild for ABI " << e.host_abi_version;
        }
        std::cout << "\n";
      }
      std::cout << "\n";
    }
  }

  if (!loaded.empty()) {
    std::cout << "Loaded plugins this session (" << loaded.size() << "):\n";
    for (const auto& p : loaded) {
      std::cout << "  " << p.plugin_id << " v" << p.plugin_version << " ("
                << p.registered_stage_names.size() << " stage(s))"
                << "\n";
    }
  }

  return 0;
}

/// Print one stage's summary line (label, role, stage name, category, owning
/// plugin). Used by both the brief and --full listings.
void print_stage_line(const orc::presenters::StageInfo& stage) {
  std::cout << "  " << stage.display_name << "  ["
            << category_label(category_of(stage)) << "]\n";
  std::cout << "    stage name: " << stage.name
            << "   (use this in --input/--filters/--output/--filter)\n";
  if (!stage.category.empty()) {
    std::cout << "    category:   " << stage.category << "\n";
  }
  std::cout << "    plugin:     "
            << (stage.is_runtime_plugin_stage
                    ? (stage.owning_plugin_id.empty() ? "<unknown>"
                                                      : stage.owning_plugin_id)
                    : "<core>")
            << "\n";
}

/// Print one stage's parameters, compactly (one line per parameter). Used
/// under --full listings and shares its formatting logic with `describe`,
/// which prints the same information at greater length for a single stage.
void print_stage_parameters_compact(orc::presenters::ProjectPresenter& presenter,
                                   const std::string& stage_name) {
  const auto params = presenter.getStageParameters(stage_name);
  if (params.empty()) {
    std::cout << "    (no parameters)\n";
    return;
  }
  for (const auto& param : params) {
    std::cout << "    - " << param.name;
    if (!param.display_name.empty() && param.display_name != param.name) {
      std::cout << " (" << param.display_name << ")";
    }
    std::cout << ": " << orc::parameter_util::type_name(param.type)
              << (param.constraints.required ? ", required" : ", optional");
    if (param.constraints.default_value.has_value()) {
      std::cout << ", default="
                << orc::parameter_util::value_to_string(
                       *param.constraints.default_value);
    }
    std::cout << "\n";
  }
}

/// Print a stage's "## Examples" section (if it has one) in a consistent,
/// prominently formatted way: a numbered list of description + copy-pasteable
/// command. Authored by whoever maintains the stage — core maintainer or
/// third-party plugin author — as plain Markdown in instructions.md; see
/// markdown_examples.h for the exact convention. Prints nothing if the
/// stage's instructions have no such section.
void print_examples(const MarkdownExamplesResult& examples) {
  if (!examples.found || examples.examples.empty()) {
    return;
  }
  std::cout << "\nExamples:\n";
  for (size_t i = 0; i < examples.examples.size(); ++i) {
    const auto& example = examples.examples[i];
    std::cout << "\n  " << (i + 1) << ". ";
    if (!example.description.empty()) {
      std::cout << example.description << "\n";
      std::cout << "     ";
    }
    // Indent every line of a multi-line command so it stays visually
    // attached to its description under the numbered entry.
    std::string indented;
    for (char c : example.command) {
      indented += c;
      if (c == '\n') {
        indented += "     ";
      }
    }
    std::cout << "$ " << indented << "\n";
  }
}


/// `filters`: list every stage (optionally restricted to one category),
/// either as a brief summary or, with `full`, with every parameter inlined
/// (like `describe`, but for every listed stage at once).
int list_stages(std::optional<StageCategory> filter_category, bool full) {
  auto stages = orc::presenters::ProjectPresenter::getAllStages();

  if (filter_category) {
    stages.erase(std::remove_if(stages.begin(), stages.end(),
                                [&](const auto& s) {
                                  return category_of(s) != *filter_category;
                                }),
                 stages.end());
  }

  if (stages.empty()) {
    std::cout << "No stages"
              << (filter_category ? std::string(" in category '") +
                                        category_label(*filter_category) + "'"
                                  : "")
              << ".\n";
    return 0;
  }

  std::sort(stages.begin(), stages.end(), [](const auto& a, const auto& b) {
    auto role_rank = [](const auto& s) {
      if (s.is_source) return 0;
      if (s.is_sink) return 1;
      return 2;
    };
    if (role_rank(a) != role_rank(b)) return role_rank(a) < role_rank(b);
    return a.display_name < b.display_name;
  });

  const std::string noun = filter_category ? category_label(*filter_category)
                                           : std::string("stages");
  std::cout << "Available " << noun << " (" << stages.size() << "):\n\n";

  orc::presenters::ProjectPresenter presenter;  // only needed for --full
  for (const auto& stage : stages) {
    print_stage_line(stage);
    if (full) {
      print_stage_parameters_compact(presenter, stage.name);
    }
    std::cout << "\n";
  }

  if (!full) {
    std::cout << "Run 'orc-cli plugins describe <stage name>' for parameter "
                 "details on one stage, or add --full to this command to "
                 "show every stage's parameters inline.\n";
  }
  return 0;
}

/// Parse a bare "--full" flag from a subcommand's own argv (argv[0] is the
/// subcommand name itself, e.g. "stages").
bool parse_full_flag(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--full") {
      return true;
    }
  }
  return false;
}

int cmd_plugins_stages(int argc, char* argv[]) {
  return list_stages(std::nullopt, parse_full_flag(argc, argv));
}

int cmd_plugins_inputs(int argc, char* argv[]) {
  return list_stages(StageCategory::kInput, parse_full_flag(argc, argv));
}

int cmd_plugins_outputs(int argc, char* argv[]) {
  return list_stages(StageCategory::kOutput, parse_full_flag(argc, argv));
}

int cmd_plugins_filters(int argc, char* argv[]) {
  return list_stages(StageCategory::kFilters, parse_full_flag(argc, argv));
}

int cmd_plugins_describe(int argc, char* argv[]) {
  // argv[0] == "describe"
  if (argc < 2) {
    std::cerr << "Usage: orc-cli plugins describe <stage name>\n";
    std::cerr << "Run 'orc-cli plugins stages' to see stage names.\n";
    return 1;
  }
  const std::string stage_name = argv[1];

  orc::presenters::ProjectPresenter presenter;
  if (!presenter.stageExists(stage_name)) {
    std::cerr << "Error: Unknown stage '" << stage_name << "'\n";
    std::cerr << "Run 'orc-cli plugins stages' to see available stages.\n";
    return 1;
  }

  // Find this stage's StageInfo for its label and role.
  const auto all_stages = orc::presenters::ProjectPresenter::getAllStages();
  const auto info_it =
      std::find_if(all_stages.begin(), all_stages.end(),
                   [&](const auto& s) { return s.name == stage_name; });

  if (info_it != all_stages.end()) {
    std::cout << info_it->display_name << "  ["
              << category_label(category_of(*info_it)) << "]\n";
    std::cout << "stage name: " << stage_name << "\n";
  } else {
    std::cout << "Stage: " << stage_name << "\n";
  }

  const std::string instructions = presenter.getStageInstructions(stage_name);
  const MarkdownExamplesResult examples = extract_examples_section(instructions);
  if (!instructions.empty()) {
    // The "## Examples" section (if any) is rendered separately, below, in a
    // consistent numbered format rather than as raw Markdown — remove it
    // here so it isn't shown twice.
    const std::string prose = examples.found
                                  ? remove_examples_section(instructions)
                                  : instructions;
    std::cout << "\n" << prose << "\n";
  }
  print_examples(examples);

  const auto params = presenter.getStageParameters(stage_name);
  if (params.empty()) {
    std::cout << "\nThis stage takes no parameters.\n";
    return 0;
  }

  std::cout << "\nParameters (" << params.size() << "):\n\n";
  for (const auto& param : params) {
    std::cout << "  " << param.name;
    if (!param.display_name.empty() && param.display_name != param.name) {
      std::cout << " (" << param.display_name << ")";
    }
    std::cout << "\n";
    std::cout << "    type:     " << orc::parameter_util::type_name(param.type)
              << "\n";
    std::cout << "    required: " << (param.constraints.required ? "yes" : "no")
              << "\n";
    if (param.constraints.default_value.has_value()) {
      std::cout << "    default:  "
                << orc::parameter_util::value_to_string(
                       *param.constraints.default_value)
                << "\n";
    }
    if (!param.constraints.allowed_strings.empty()) {
      std::cout << "    allowed:  ";
      for (size_t i = 0; i < param.constraints.allowed_strings.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << param.constraints.allowed_strings[i];
      }
      std::cout << "\n";
    }
    if (!param.description.empty()) {
      std::cout << "    " << param.description << "\n";
    }
    std::cout << "\n";
  }
  return 0;
}

int cmd_plugins_add(int argc, char* argv[]) {
  // argv[0] = "add", argv[1] = <path>, remaining = options
  if (argc < 2) {
    std::cerr << "Error: 'add' requires a plugin path\n";
    std::cerr << "Usage: orc-cli plugins add <path> [--id ID] [--version VER] "
                 "[--license SPDX] [--trusted]\n";
    return 1;
  }

  std::string path = argv[1];
  std::string plugin_id;
  std::string plugin_version;
  std::string license_spdx;
  bool trusted = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--id" && i + 1 < argc) {
      plugin_id = argv[++i];
    } else if (arg == "--version" && i + 1 < argc) {
      plugin_version = argv[++i];
    } else if (arg == "--license" && i + 1 < argc) {
      license_spdx = argv[++i];
    } else if (arg == "--trusted") {
      trusted = true;
    } else {
      std::cerr << "Error: Unknown option: " << arg << "\n";
      return 1;
    }
  }

  const auto result = orc::presenters::ProjectPresenter::addPluginToRegistry(
      path, plugin_id, plugin_version, license_spdx, false, trusted);

  if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    return 1;
  }

  std::cout << "Plugin added to registry.\n";
  std::cout << "Note: Changes take effect on the next application launch.\n";
  return 0;
}

int cmd_plugins_remove(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Error: 'remove' requires a plugin id\n";
    std::cerr << "Usage: orc-cli plugins remove <id>\n";
    return 1;
  }

  const std::string plugin_id = argv[1];
  const auto result =
      orc::presenters::ProjectPresenter::removePluginFromRegistry(plugin_id);

  if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    return 1;
  }

  std::cout << "Plugin '" << plugin_id << "' removed from registry.\n";
  std::cout << "Note: Changes take effect on the next application launch.\n";
  return 0;
}

int cmd_plugins_set_enabled(int argc, char* argv[], bool enabled) {
  const std::string subcommand = enabled ? "enable" : "disable";
  if (argc < 2) {
    std::cerr << "Error: '" << subcommand << "' requires a plugin id\n";
    std::cerr << "Usage: orc-cli plugins " << subcommand << " <id>\n";
    return 1;
  }

  const std::string plugin_id = argv[1];
  const auto result =
      orc::presenters::ProjectPresenter::setPluginRegistryEntryEnabled(
          plugin_id, enabled);

  if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    return 1;
  }

  std::cout << "Plugin '" << plugin_id << "' "
            << (enabled ? "enabled" : "disabled") << ".\n";
  std::cout << "Note: Changes take effect on the next application launch.\n";
  return 0;
}

int cmd_plugins_set_trusted(int argc, char* argv[], bool trusted) {
  const std::string subcommand = trusted ? "trust" : "untrust";
  if (argc < 2) {
    std::cerr << "Error: '" << subcommand << "' requires a plugin id\n";
    std::cerr << "Usage: orc-cli plugins " << subcommand << " <id>\n";
    return 1;
  }

  const std::string plugin_id = argv[1];
  const auto result =
      orc::presenters::ProjectPresenter::setPluginRegistryEntryTrusted(
          plugin_id, trusted);

  if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    return 1;
  }

  std::cout << "Plugin '" << plugin_id << "' marked as "
            << (trusted ? "trusted" : "untrusted") << ".\n";
  std::cout << "Note: Changes take effect on the next application launch.\n";
  return 0;
}

// Print a one-line note about how the index was sourced (live vs cache) so the
// user understands staleness when offline.
void print_index_status(const orc::presenters::PluginIndexInfo& index) {
  if (index.offline && index.from_cache) {
    std::cout << "(offline — showing the last cached index)\n\n";
  } else if (index.offline) {
    std::cout << "(offline — no cached index available)\n\n";
  }
}

int cmd_plugins_search(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Error: 'search' requires a search term\n";
    std::cerr << "Usage: orc-cli plugins search <term>\n";
    return 1;
  }
  const std::string term = argv[1];
  const auto index = orc::presenters::ProjectPresenter::readPluginIndex();
  if (!index.available) {
    std::cerr << "Error: "
              << (index.error_message.empty()
                      ? "the plugin index could not be loaded"
                      : index.error_message)
              << "\n";
    return 1;
  }

  print_index_status(index);

  size_t matches = 0;
  const std::string needle = [&term]() {
    std::string lower = term;
    for (char& ch : lower) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lower;
  }();
  auto contains_ci = [&needle](const std::string& text) {
    std::string lower = text;
    for (char& ch : lower) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lower.find(needle) != std::string::npos;
  };

  for (const auto& e : index.entries) {
    bool hit = contains_ci(e.id) || contains_ci(e.display_name) ||
               contains_ci(e.description);
    for (const auto& tag : e.tags) {
      hit = hit || contains_ci(tag);
    }
    if (!hit) {
      continue;
    }
    ++matches;
    std::cout << e.id << "  " << e.display_name << "\n";
    if (!e.description.empty()) {
      std::cout << "    " << e.description << "\n";
    }
    std::cout << "    "
              << (e.has_compatible_build
                      ? "compatible with this host"
                      : (e.compatibility_message.empty()
                             ? "no compatible build for this host"
                             : e.compatibility_message))
              << (e.already_installed ? "; already installed" : "") << "\n";
  }

  if (matches == 0) {
    std::cout << "No plugins matched '" << term << "'.\n";
  }
  return 0;
}

int cmd_plugins_info(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Error: 'info' requires a plugin id\n";
    std::cerr << "Usage: orc-cli plugins info <id>\n";
    return 1;
  }
  const std::string plugin_id = argv[1];
  const auto index = orc::presenters::ProjectPresenter::readPluginIndex();
  if (!index.available) {
    std::cerr << "Error: "
              << (index.error_message.empty()
                      ? "the plugin index could not be loaded"
                      : index.error_message)
              << "\n";
    return 1;
  }

  const orc::presenters::PluginIndexEntryInfo* entry = nullptr;
  for (const auto& e : index.entries) {
    if (e.id == plugin_id) {
      entry = &e;
      break;
    }
  }
  if (entry == nullptr) {
    std::cerr << "Error: no plugin with id '" << plugin_id
              << "' is listed in the index\n";
    return 1;
  }

  print_index_status(index);

  std::cout << "id:          " << entry->id << "\n";
  std::cout << "name:        " << entry->display_name << "\n";
  if (!entry->description.empty()) {
    std::cout << "description: " << entry->description << "\n";
  }
  if (!entry->maintainer.empty()) {
    std::cout << "maintainer:  " << entry->maintainer << "\n";
  }
  if (!entry->license_spdx.empty()) {
    std::cout << "license:     " << entry->license_spdx << "\n";
  }
  if (!entry->source_repo_url.empty()) {
    std::cout << "source:      " << entry->source_repo_url << "\n";
  }
  if (!entry->tags.empty()) {
    std::cout << "tags:        ";
    for (size_t i = 0; i < entry->tags.size(); ++i) {
      std::cout << (i == 0 ? "" : ", ") << entry->tags[i];
    }
    std::cout << "\n";
  }
  std::cout << "compatible:  "
            << (entry->has_compatible_build
                    ? "yes"
                    : (entry->compatibility_message.empty()
                           ? "no"
                           : "no — " + entry->compatibility_message))
            << "\n";
  std::cout << "installed:   " << (entry->already_installed ? "yes" : "no")
            << "\n";
  std::cout << "builds (" << entry->artifacts.size() << "):\n";
  for (const auto& a : entry->artifacts) {
    std::cout << "  - platform: " << a.platform << ", ABI " << a.host_abi
              << ", version " << a.plugin_version << "\n";
  }
  return 0;
}

int cmd_plugins_install(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Error: 'install' requires a plugin id\n";
    std::cerr << "Usage: orc-cli plugins install <id>\n";
    return 1;
  }
  const std::string plugin_id = argv[1];
  const auto result =
      orc::presenters::ProjectPresenter::installIndexedPlugin(plugin_id);

  if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    return 1;
  }

  std::cout << "Plugin '" << plugin_id
            << "' added to the registry (untrusted).\n";
  std::cout << "Trust it before it will be downloaded and loaded:\n";
  std::cout << "  orc-cli plugins trust " << plugin_id << "\n";
  std::cout << "Note: Changes take effect on the next application launch.\n";
  return 0;
}

}  // namespace

int plugins_command(int argc, char* argv[]) {
  // argv[0] = "plugins"
  if (argc < 2) {
    print_plugins_usage(argv[0]);
    return 1;
  }

  const std::string subcommand = argv[1];

  if (subcommand == "--help" || subcommand == "-h") {
    print_plugins_usage(argv[0]);
    return 0;
  }

  if (subcommand == "list") {
    return cmd_plugins_list();
  }

  if (subcommand == "stages") {
    return cmd_plugins_stages(argc - 1, argv + 1);
  }

  if (subcommand == "inputs") {
    return cmd_plugins_inputs(argc - 1, argv + 1);
  }

  if (subcommand == "outputs") {
    return cmd_plugins_outputs(argc - 1, argv + 1);
  }

  if (subcommand == "filters") {
    return cmd_plugins_filters(argc - 1, argv + 1);
  }

  if (subcommand == "describe") {
    return cmd_plugins_describe(argc - 1, argv + 1);
  }

  if (subcommand == "add") {
    // Pass remaining args starting from "add"
    return cmd_plugins_add(argc - 1, argv + 1);
  }

  if (subcommand == "remove") {
    return cmd_plugins_remove(argc - 1, argv + 1);
  }

  if (subcommand == "enable") {
    return cmd_plugins_set_enabled(argc - 1, argv + 1, true);
  }

  if (subcommand == "disable") {
    return cmd_plugins_set_enabled(argc - 1, argv + 1, false);
  }

  if (subcommand == "trust") {
    return cmd_plugins_set_trusted(argc - 1, argv + 1, true);
  }

  if (subcommand == "untrust") {
    return cmd_plugins_set_trusted(argc - 1, argv + 1, false);
  }

  if (subcommand == "search") {
    return cmd_plugins_search(argc - 1, argv + 1);
  }

  if (subcommand == "info") {
    return cmd_plugins_info(argc - 1, argv + 1);
  }

  if (subcommand == "install") {
    return cmd_plugins_install(argc - 1, argv + 1);
  }

  std::cerr << "Error: Unknown plugins subcommand: " << subcommand << "\n\n";
  print_plugins_usage(argv[0]);
  return 1;
}

}  // namespace cli
}  // namespace orc
