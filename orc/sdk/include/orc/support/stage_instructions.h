/*
 * File:        stage_instructions.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Runtime loader for a stage's instructions.md (platform file I/O)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

// SDK TIER: support — compiled-into-plugin utility. NOT part of the binary
// ABI; changes never force an ABI bump (recompile the plugin at your leisure).
//
// This header carries the only file-I/O / OS-header dependency in the SDK
// (<fstream>, <dlfcn.h>/<windows.h>). It is deliberately isolated in the
// support tier so the stability-critical abi/ and stage/ contract headers stay
// free of platform I/O.

#include <fstream>
#include <sstream>
#include <string>

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
