/*
 * File:        orc_stage_tooling.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Canonical stage helper/tooling contracts for plugin stages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 *
 * STABILITY: PUBLIC — This header is part of the stable plugin SDK.
 */

#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace orc::plugin {

// Read the instructions.md file that lives alongside the calling plugin's
// shared library.  plugin_symbol must be an address within the calling plugin
// (e.g. a static local variable from the get_instructions() body) so that
// dladdr / GetModuleHandleEx can identify the correct shared library.
//
// Returns the UTF-8 text of the file, or an empty string if it cannot be found.
inline std::string read_stage_instructions(const void* plugin_symbol) {
  std::string lib_path;

#if defined(__unix__) || defined(__APPLE__)
  ::Dl_info info{};
  if (::dladdr(plugin_symbol, &info) != 0 && info.dli_fname != nullptr) {
    lib_path = info.dli_fname;
  }
#elif defined(_WIN32)
  HMODULE hmod = nullptr;
  if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(plugin_symbol), &hmod) &&
      hmod != nullptr) {
    char buf[MAX_PATH] = {};
    if (::GetModuleFileNameA(hmod, buf, MAX_PATH) > 0) {
      lib_path = buf;
    }
  }
#endif

  if (lib_path.empty()) return {};

  // Replace the shared-library extension with ".md".
  const auto dot = lib_path.rfind('.');
  const std::string md_path =
      (dot != std::string::npos ? lib_path.substr(0, dot) : lib_path) + ".md";

  std::ifstream f(md_path);
  if (!f.is_open()) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace orc::plugin

namespace orc {

// Canonical helper/tool categories that allow host GUI/CLI to route tooling
// requests without hardcoded stage-id branches.
enum class StageToolKind {
  ConfigDialog,
  NonModalEditor,
  BatchAnalysis,
  PreviewUtility,
  Custom
};

struct StageToolDescriptor {
  std::string tool_id;
  std::string display_name;
  std::string description;
  StageToolKind kind{StageToolKind::Custom};

  // If true, host may present this tool as a long-lived window/editor.
  bool non_modal{false};

  // Optional stable contract string for tool payload semantics.
  std::string contract_id;
};

// Canonical analysis-tool descriptor contract used when a stage advertises
// long-running/batch analysis capabilities.
struct AnalysisToolDescriptor {
  // Stable identifier consumed by host presenters and UI routing.
  std::string tool_id;

  // Human-friendly metadata for menus and dialogs.
  std::string display_name;
  std::string description;

  // Stable payload/execution contract string (for example,
  // "decode-orc.stage-tools.dropout-analysis.v1").
  std::string contract_id;

  // Whether host should automatically request analysis data after a trigger.
  bool auto_request_after_trigger{true};
};

// ---------------------------------------------------------------------------
// Stage documentation
// ---------------------------------------------------------------------------

// Preferred form: reads instructions.md from alongside the plugin .so at
// runtime.  No inline string needed in any source file — instructions.md is
// the single source of truth.
//
// Usage (in the class body):
//   class MyStage : public DAGStage {
//    public:
//     ORC_STAGE_INSTRUCTIONS_MD
//   };
//
// The build system (orc_add_stage_plugin) automatically copies instructions.md
// next to the plugin shared library so it can be found at runtime.
// clang-format off
#define ORC_STAGE_INSTRUCTIONS_MD                                        \
  std::string get_instructions() const override {                        \
    static const char kSentinel_ = '\0';                                 \
    return ::orc::plugin::read_stage_instructions(                       \
        static_cast<const void*>(&kSentinel_));                          \
  }
// clang-format on

// Legacy form: embed instructions inline as a string literal.  Kept for
// backward compatibility with external plugins that do not ship an
// instructions.md file alongside their shared library.
//
//   class MyStage : public DAGStage {
//    public:
//     ORC_STAGE_INSTRUCTIONS(kInstructions_)
//    private:
//     static const char kInstructions_[];
//   };
//   const char MyStage::kInstructions_[] = R"(# My Stage ...)";
#define ORC_STAGE_INSTRUCTIONS(content) \
  std::string get_instructions() const override { return (content); }

// Optional mixin: stages can implement this to declare supported helper/tool
// integrations in a host-agnostic way.
class StageToolProvider {
 public:
  virtual ~StageToolProvider() = default;
  virtual std::vector<StageToolDescriptor> get_stage_tools() const = 0;
};

// Optional mixin: stages can implement this to provide explicit analysis-tool
// contracts in addition to generic StageToolDescriptor metadata.
class AnalysisToolProvider {
 public:
  virtual ~AnalysisToolProvider() = default;
  virtual std::vector<AnalysisToolDescriptor> get_analysis_tools() const = 0;
};

}  // namespace orc
