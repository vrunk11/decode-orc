/*
 * File:        plugin_index.h
 * Module:      orc-core
 * Purpose:     Data model and YAML parser for the curated plugin index
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#if defined(ORC_GUI_BUILD) || defined(ORC_CLI_BUILD)
#error \
    "plugin_index.h is a core-only header. Access plugin discovery through ProjectPresenter."
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace orc {

/// Schema major version understood by this host. Additions within a major
/// version are non-breaking (unknown fields are ignored); a higher major is
/// parsed best-effort with a warning so older hosts tolerate newer indexes.
inline constexpr int kPluginIndexSchemaVersion = 1;

/**
 * @brief A single downloadable build of a plugin, keyed by platform and ABI.
 */
struct PluginIndexArtifact {
  std::string platform;   ///< Platform token: "linux", "macos", or "windows"
                          ///< (a more specific value such as "linux-x86_64" is
                          ///< matched by platform prefix).
  uint32_t host_abi = 0;  ///< Host ABI this build targets; 0 means unspecified.
  std::string url;        ///< Direct release-asset download URL.
  std::string sha256;     ///< Mandatory 64-hex digest of the artifact.
  std::string plugin_version;        ///< Plugin release version.
  std::string min_host_app_version;  ///< Minimum host application version.
  std::string asset_name;  ///< Optional explicit artifact filename; derived
                           ///< from the URL when empty.
};

/**
 * @brief One plugin as advertised by the curated index.
 */
struct PluginIndexEntry {
  std::string id;            ///< Unique plugin identifier.
  std::string display_name;  ///< Human-readable name.
  std::string description;   ///< Short description.
  std::vector<std::string> tags;
  std::string maintainer;
  std::string license_spdx;
  std::string source_repo_url;
  std::vector<PluginIndexArtifact> artifacts;
};

/**
 * @brief Parsed curated plugin index.
 */
struct PluginIndex {
  int schema_version = 0;
  std::vector<PluginIndexEntry> entries;
};

/**
 * @brief Outcome of parsing an index document.
 */
struct PluginIndexParseResult {
  bool success = false;
  PluginIndex index;
  std::vector<std::string> warnings;
  std::string error_message;
};

/**
 * @brief Parse a curated plugin index from YAML text.
 *
 * Forward-compatible: unknown top-level, per-entry, and per-artifact fields are
 * ignored, and a schema major greater than the host's known version is parsed
 * best-effort with a warning. Malformed YAML fails with an error_message.
 */
PluginIndexParseResult parse_plugin_index_yaml(const std::string& yaml_text);

/**
 * @brief Serialize an index back to YAML (used to persist the local cache).
 */
std::string serialize_plugin_index_yaml(const PluginIndex& index);

}  // namespace orc
