/*
 * File:        dropout_presenter.cpp
 * Module:      orc-presenters
 * Purpose:     Dropout presenter implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "dropout_presenter.h"

#include <orc/support/logging.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include "../core/include/project.h"
#include "i_project_presenter.h"

namespace orc::presenters {

namespace {

std::map<uint64_t, FrameDropoutMap> parse_dropout_map_string(
    const std::string& map_str) {
  std::map<uint64_t, FrameDropoutMap> result;

  if (map_str.empty() || map_str == "[]") {
    return result;
  }

  size_t pos = 0;
  auto skip_whitespace = [&]() {
    while (pos < map_str.length() &&
           std::isspace(static_cast<unsigned char>(map_str[pos]))) {
      ++pos;
    }
  };

  auto expect_char = [&](char c) -> bool {
    skip_whitespace();
    if (pos < map_str.length() && map_str[pos] == c) {
      ++pos;
      return true;
    }
    return false;
  };

  auto parse_uint = [&]() -> uint32_t {
    skip_whitespace();
    uint32_t value = 0;
    while (pos < map_str.length() &&
           std::isdigit(static_cast<unsigned char>(map_str[pos]))) {
      value = value * 10 + static_cast<uint32_t>(map_str[pos] - '0');
      ++pos;
    }
    return value;
  };

  auto parse_dropout_region = [&]() -> DropoutRegion {
    DropoutRegion region;
    region.basis = DropoutRegion::DetectionBasis::HINT_DERIVED;

    if (!expect_char('{')) {
      return region;
    }

    while (pos < map_str.length() && map_str[pos] != '}') {
      skip_whitespace();

      std::string key;
      while (pos < map_str.length() &&
             std::isalpha(static_cast<unsigned char>(map_str[pos]))) {
        key += map_str[pos++];
      }

      if (!expect_char(':')) {
        break;
      }

      if (key == "line") {
        region.line = parse_uint();
      } else if (key == "start") {
        region.start_sample = parse_uint();
      } else if (key == "end") {
        region.end_sample = parse_uint();
      }

      expect_char(',');
    }

    expect_char('}');
    return region;
  };

  auto parse_dropout_list = [&]() -> std::vector<DropoutRegion> {
    std::vector<DropoutRegion> regions;
    if (!expect_char('[')) {
      return regions;
    }

    while (pos < map_str.length() && map_str[pos] != ']') {
      skip_whitespace();
      if (pos < map_str.length() && map_str[pos] == '{') {
        regions.push_back(parse_dropout_region());
      }
      expect_char(',');
      skip_whitespace();
    }

    expect_char(']');
    return regions;
  };

  if (!expect_char('[')) {
    ORC_LOG_ERROR("DropoutPresenter: dropout_map must start with '['");
    return result;
  }

  while (pos < map_str.length() && map_str[pos] != ']') {
    skip_whitespace();

    if (!expect_char('{')) {
      break;
    }

    FrameDropoutMap frame_map;

    while (pos < map_str.length() && map_str[pos] != '}') {
      skip_whitespace();

      std::string key;
      while (pos < map_str.length() &&
             std::isalpha(static_cast<unsigned char>(map_str[pos]))) {
        key += map_str[pos++];
      }

      if (!expect_char(':')) {
        break;
      }

      if (key == "frame") {
        frame_map.frame_id = static_cast<FrameID>(parse_uint());
      } else if (key == "add") {
        frame_map.additions = parse_dropout_list();
      } else if (key == "remove") {
        frame_map.removals = parse_dropout_list();
      }

      expect_char(',');
    }

    expect_char('}');
    result[frame_map.frame_id] = frame_map;
    expect_char(',');
  }

  return result;
}

std::string encode_dropout_map_string(
    const std::map<uint64_t, FrameDropoutMap>& map) {
  if (map.empty()) {
    return "[]";
  }

  std::ostringstream oss;
  oss << "[";

  bool first_frame = true;
  for (const auto& [frame_num, frame_map] : map) {
    if (!first_frame) {
      oss << ",";
    }
    first_frame = false;

    oss << "{frame:" << frame_num;

    if (!frame_map.additions.empty()) {
      oss << ",add:[";
      bool first_region = true;
      for (const auto& region : frame_map.additions) {
        if (!first_region) {
          oss << ",";
        }
        first_region = false;
        oss << "{line:" << region.line << ",start:" << region.start_sample
            << ",end:" << region.end_sample << "}";
      }
      oss << "]";
    }

    if (!frame_map.removals.empty()) {
      oss << ",remove:[";
      bool first_region = true;
      for (const auto& region : frame_map.removals) {
        if (!first_region) {
          oss << ",";
        }
        first_region = false;
        oss << "{line:" << region.line << ",start:" << region.start_sample
            << ",end:" << region.end_sample << "}";
      }
      oss << "]";
    }

    oss << "}";
  }

  oss << "]";
  return oss.str();
}

}  // namespace

class DropoutPresenter::Impl {
 public:
  explicit Impl(orc::presenters::IProjectPresenter& project_presenter)
      : project_presenter_(project_presenter) {}

  orc::presenters::IProjectPresenter& project_presenter_;
};

DropoutPresenter::DropoutPresenter(
    orc::presenters::IProjectPresenter& project_presenter)
    : impl_(std::make_unique<Impl>(project_presenter)) {}

DropoutPresenter::~DropoutPresenter() = default;

DropoutPresenter::DropoutPresenter(DropoutPresenter&&) noexcept = default;
DropoutPresenter& DropoutPresenter::operator=(DropoutPresenter&&) noexcept =
    default;

std::map<uint64_t, FrameDropoutMap> DropoutPresenter::getDropoutMap(
    NodeID node_id) {
  try {
    const auto& nodes = impl_->project_presenter_.getNodes();
    auto node_it = std::find_if(nodes.begin(), nodes.end(),
                                [&node_id](const orc::presenters::NodeInfo& n) {
                                  return n.node_id == node_id;
                                });

    if (node_it == nodes.end()) {
      ORC_LOG_WARN("Node {} not found in project", node_id.to_string());
      return {};
    }

    if (node_it->stage_name != "dropout_map") {
      ORC_LOG_WARN("Node {} is not a dropout_map stage (it's {})",
                   node_id.to_string(), node_it->stage_name);
      return {};
    }

    auto params = impl_->project_presenter_.getNodeParameters(node_id);
    auto it = params.find("dropout_map");
    if (it == params.end()) {
      return {};
    }

    std::string map_str = std::get<std::string>(it->second);
    return parse_dropout_map_string(map_str);
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Error getting dropout map: {}", e.what());
    return {};
  }
}

bool DropoutPresenter::setDropoutMap(
    NodeID node_id, const std::map<uint64_t, FrameDropoutMap>& dropout_map) {
  try {
    std::string map_str = encode_dropout_map_string(dropout_map);

    std::map<std::string, orc::ParameterValue> params;
    params["dropout_map"] = map_str;

    impl_->project_presenter_.setNodeParameters(node_id, params);

    return true;
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Error setting dropout map: {}", e.what());
    return false;
  }
}

}  // namespace orc::presenters
