/*
 * File:        pluginbrowsemodel.cpp
 * Module:      orc-gui
 * Purpose:     Presenter-boundary model for the curated plugin browse dialog
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pluginbrowsemodel.h"

#include <algorithm>
#include <cctype>

namespace orc {
namespace {

std::string to_lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool contains_ci(const std::string& haystack, const std::string& needle_lower) {
  return to_lower(haystack).find(needle_lower) != std::string::npos;
}

}  // namespace

std::vector<orc::presenters::PluginIndexEntryInfo> PluginBrowseModel::search(
    const std::string& term) const {
  const std::string needle = to_lower(term);
  if (needle.empty()) {
    return index_.entries;
  }
  std::vector<orc::presenters::PluginIndexEntryInfo> matches;
  for (const auto& entry : index_.entries) {
    bool hit = contains_ci(entry.id, needle) ||
               contains_ci(entry.display_name, needle) ||
               contains_ci(entry.description, needle);
    if (!hit) {
      for (const auto& tag : entry.tags) {
        if (contains_ci(tag, needle)) {
          hit = true;
          break;
        }
      }
    }
    if (hit) {
      matches.push_back(entry);
    }
  }
  return matches;
}

std::string PluginBrowseModel::statusMessage() const {
  if (!index_.available) {
    return index_.error_message.empty() ? "The plugin index is unavailable."
                                        : index_.error_message;
  }
  const std::string count = std::to_string(index_.entries.size());
  if (index_.offline && index_.from_cache) {
    return "Offline — showing " + count + " plugins from the cached index.";
  }
  return count + " plugins available.";
}

}  // namespace orc
