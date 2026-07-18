/*
 * File:        plugin_index.cpp
 * Module:      orc-core
 * Purpose:     Data model and YAML parser for the curated plugin index
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "include/plugin_index.h"

#include <yaml-cpp/yaml.h>

namespace orc {
namespace {

PluginIndexArtifact parse_artifact(const YAML::Node& node,
                                   std::vector<std::string>& warnings,
                                   const std::string& plugin_id) {
  PluginIndexArtifact artifact;
  artifact.platform = node["platform"].as<std::string>("");
  artifact.host_abi = node["host_abi"].as<uint32_t>(0);
  artifact.url = node["url"].as<std::string>("");
  artifact.sha256 = node["sha256"].as<std::string>("");
  artifact.plugin_version = node["plugin_version"].as<std::string>("");
  artifact.min_host_app_version =
      node["min_host_app_version"].as<std::string>("");
  artifact.asset_name = node["asset_name"].as<std::string>("");

  if (artifact.sha256.empty()) {
    warnings.push_back("Plugin '" + plugin_id + "' artifact for platform '" +
                       artifact.platform +
                       "' is missing a mandatory sha256 digest");
  }
  return artifact;
}

PluginIndexEntry parse_entry(const YAML::Node& node,
                             std::vector<std::string>& warnings) {
  PluginIndexEntry entry;
  entry.id = node["id"].as<std::string>("");
  entry.display_name = node["display_name"].as<std::string>("");
  entry.description = node["description"].as<std::string>("");
  entry.maintainer = node["maintainer"].as<std::string>("");
  entry.license_spdx = node["license_spdx"].as<std::string>("");
  entry.source_repo_url = node["source_repo_url"].as<std::string>("");

  const YAML::Node tags_node = node["tags"];
  if (tags_node && tags_node.IsSequence()) {
    for (const auto& tag : tags_node) {
      entry.tags.push_back(tag.as<std::string>(""));
    }
  }

  const YAML::Node artifacts_node = node["artifacts"];
  if (artifacts_node && artifacts_node.IsSequence()) {
    for (const auto& artifact_node : artifacts_node) {
      entry.artifacts.push_back(
          parse_artifact(artifact_node, warnings, entry.id));
    }
  }

  if (entry.license_spdx.empty()) {
    warnings.push_back("Plugin '" + entry.id +
                       "' is missing an SPDX license identifier");
  }
  return entry;
}

}  // namespace

PluginIndexParseResult parse_plugin_index_yaml(const std::string& yaml_text) {
  PluginIndexParseResult result;

  YAML::Node root;
  try {
    root = YAML::Load(yaml_text);
  } catch (const YAML::Exception& e) {
    result.error_message =
        std::string("Failed to parse plugin index YAML: ") + e.what();
    return result;
  }

  if (!root || !root.IsMap()) {
    result.error_message = "Plugin index root is not a mapping";
    return result;
  }

  // The schema version is authoritative for forward compatibility. Unknown
  // additions within a major version are ignored; a newer major is parsed
  // best-effort so an older host still resolves compatible builds.
  result.index.schema_version =
      root["registry_schema"].as<int>(kPluginIndexSchemaVersion);
  if (result.index.schema_version > kPluginIndexSchemaVersion) {
    result.warnings.push_back("Plugin index uses schema version " +
                              std::to_string(result.index.schema_version) +
                              " which is newer than this host understands (" +
                              std::to_string(kPluginIndexSchemaVersion) +
                              "); unknown entries may be skipped");
  }

  const YAML::Node plugins_node = root["plugins"];
  if (!plugins_node || !plugins_node.IsSequence()) {
    // A well-formed but empty index is valid.
    result.success = true;
    return result;
  }

  for (const auto& plugin_node : plugins_node) {
    if (!plugin_node.IsMap()) {
      continue;
    }
    PluginIndexEntry entry = parse_entry(plugin_node, result.warnings);
    if (entry.id.empty()) {
      result.warnings.push_back("Skipping plugin index entry with no id");
      continue;
    }
    result.index.entries.push_back(std::move(entry));
  }

  result.success = true;
  return result;
}

}  // namespace orc
