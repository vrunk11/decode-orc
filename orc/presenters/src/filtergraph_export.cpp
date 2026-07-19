/*
 * File:        filtergraph_export.cpp
 * Module:      orc-presenters
 * Purpose:     Implementation of the project -> filtergraph string exporter.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "filtergraph_export.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "i_project_presenter.h"
#include "project_presenter_types.h"

namespace orc {
namespace presenters {

namespace {

/// Characters that require a value to be quoted (or escaped) so the result
/// round-trips through parse_filtergraph() unchanged. Mirrors exactly the
/// separator/special-character set the parser treats specially.
bool needs_quoting(const std::string& value) {
  if (value.empty()) {
    return true;  // An empty value must still be distinguishable/parseable.
  }
  return value.find_first_of(" \t:,;[]'\"\\") != std::string::npos;
}

/// Quote or escape `value` so it reads back identically via
/// parse_filtergraph(). Prefers wrapping in whichever quote character
/// (single or double) does not itself appear in the value; if both appear,
/// falls back to escaping each special character individually.
std::string quote_value_if_needed(const std::string& value) {
  if (!needs_quoting(value)) {
    return value;
  }

  const bool has_single = value.find('\'') != std::string::npos;
  const bool has_double = value.find('"') != std::string::npos;

  if (!has_single) {
    return "'" + value + "'";
  }
  if (!has_double) {
    return "\"" + value + "\"";
  }

  // Contains both quote characters: escape each special character in place
  // instead of quoting.
  static const std::string kSpecial = ":,;[]'\" \t\\";
  std::string escaped;
  for (char c : value) {
    if (kSpecial.find(c) != std::string::npos) {
      escaped += '\\';
    }
    escaped += c;
  }
  return escaped;
}

/// A synthetic, always-valid link label for a node, e.g. "n0", "n1", ...
/// (never needs quoting: labels are a closed set of ASCII identifiers we
/// generate ourselves).
std::string label_for(size_t index) { return "n" + std::to_string(index); }

}  // namespace

std::string export_project_as_filtergraph(IProjectPresenter& presenter) {
  const std::vector<NodeInfo> nodes = presenter.getNodes();
  if (nodes.empty()) {
    return "";
  }
  const std::vector<EdgeInfo> edges = presenter.getEdges();

  // Map each node's NodeID (via its string form) to a stable index/label,
  // and to its NodeInfo, in one pass.
  std::map<std::string, size_t> node_index_by_id;
  for (size_t i = 0; i < nodes.size(); ++i) {
    node_index_by_id[nodes[i].node_id.to_string()] = i;
  }

  // For each node, collect the labels of every node that feeds into it
  // (its input labels) and whether anything consumes its output.
  std::vector<std::vector<std::string>> input_labels(nodes.size());
  std::vector<bool> has_output(nodes.size(), false);
  for (const auto& edge : edges) {
    auto source_it = node_index_by_id.find(edge.source_node.to_string());
    auto target_it = node_index_by_id.find(edge.target_node.to_string());
    if (source_it == node_index_by_id.end() ||
        target_it == node_index_by_id.end()) {
      continue;  // Defensive: skip an edge referencing an unknown node.
    }
    has_output[source_it->second] = true;
    input_labels[target_it->second].push_back(label_for(source_it->second));
  }

  // List nodes with no incoming edges (sources) first, for readability;
  // stable otherwise, since the grammar itself does not require any
  // particular order (labels resolve globally).
  std::vector<size_t> order(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const bool a_is_source = input_labels[a].empty();
    const bool b_is_source = input_labels[b].empty();
    return a_is_source && !b_is_source;
  });

  std::string result;
  bool first_chain = true;
  for (size_t idx : order) {
    if (!first_chain) {
      result += "; ";
    }
    first_chain = false;

    for (const auto& in_label : input_labels[idx]) {
      result += "[" + in_label + "]";
    }

    result += nodes[idx].stage_name;

    const std::map<std::string, ParameterValue> params =
        presenter.getNodeParameters(nodes[idx].node_id);
    if (!params.empty()) {
      result += "=";
      bool first_param = true;
      for (const auto& [key, value] : params) {
        if (!first_param) {
          result += ":";
        }
        first_param = false;
        result += key + "=" +
                 quote_value_if_needed(parameter_util::value_to_string(value));
      }
    }

    if (has_output[idx]) {
      result += "[" + label_for(idx) + "]";
    }
  }

  return result;
}

}  // namespace presenters
}  // namespace orc
