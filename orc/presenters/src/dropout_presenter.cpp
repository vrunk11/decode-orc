/*
 * File:        dropout_presenter.cpp
 * Module:      orc-presenters
 * Purpose:     Dropout presenter implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "dropout_presenter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include "../core/include/artifact.h"
#include "../core/include/dropout_util.h"
#include "../core/include/logging.h"
#include "../core/include/project.h"
#include "../core/include/video_frame_representation.h"
#include "i_project_presenter.h"

namespace orc::presenters {

namespace {

std::map<uint64_t, FieldDropoutMap> parse_dropout_map_string(
    const std::string& map_str) {
  std::map<uint64_t, FieldDropoutMap> result;

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

    FieldDropoutMap field_map;

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

      if (key == "field") {
        field_map.field_id = FieldID(parse_uint());
      } else if (key == "add") {
        field_map.additions = parse_dropout_list();
      } else if (key == "remove") {
        field_map.removals = parse_dropout_list();
      }

      expect_char(',');
    }

    expect_char('}');
    result[field_map.field_id.value()] = field_map;
    expect_char(',');
  }

  return result;
}

std::string encode_dropout_map_string(
    const std::map<uint64_t, FieldDropoutMap>& map) {
  if (map.empty()) {
    return "[]";
  }

  std::ostringstream oss;
  oss << "[";

  bool first_field = true;
  for (const auto& [field_num, field_map] : map) {
    if (!first_field) {
      oss << ",";
    }
    first_field = false;

    oss << "{field:" << field_num;

    if (!field_map.additions.empty()) {
      oss << ",add:[";
      bool first_region = true;
      for (const auto& region : field_map.additions) {
        if (!first_region) {
          oss << ",";
        }
        first_region = false;
        oss << "{line:" << region.line << ",start:" << region.start_sample
            << ",end:" << region.end_sample << "}";
      }
      oss << "]";
    }

    if (!field_map.removals.empty()) {
      oss << ",remove:[";
      bool first_region = true;
      for (const auto& region : field_map.removals) {
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
};  // DEBUG: Extra brace was needed here!

DropoutPresenter::DropoutPresenter(
    orc::presenters::IProjectPresenter& project_presenter)
    : impl_(std::make_unique<Impl>(project_presenter)) {}

DropoutPresenter::~DropoutPresenter() = default;

DropoutPresenter::DropoutPresenter(DropoutPresenter&&) noexcept = default;
DropoutPresenter& DropoutPresenter::operator=(DropoutPresenter&&) noexcept =
    default;

std::vector<DetectedDropout> DropoutPresenter::detectDropouts(
    NodeID node_id, FieldID field_id) {
  return {};
}

std::vector<DetectedDropout> DropoutPresenter::getDetectedDropouts(
    NodeID node_id, FieldID field_id) const {
  return {};
}

void DropoutPresenter::clearDetections(NodeID node_id, FieldID field_id) {}

void DropoutPresenter::updateDropoutDecision(
    NodeID node_id, FieldID field_id, int line, int pixel_start,
    DropoutDecision decision, const std::string& correction_method) {}

std::vector<DropoutCorrection> DropoutPresenter::getCorrections(
    NodeID node_id, FieldID field_id) const {
  return {};
}

void DropoutPresenter::removeCorrection(NodeID node_id, FieldID field_id,
                                        int line, int pixel_start) {}

void DropoutPresenter::clearCorrections(NodeID node_id, FieldID field_id) {}

FieldDropoutStats DropoutPresenter::getFieldStats(NodeID node_id,
                                                  FieldID field_id) const {
  return FieldDropoutStats{};
}

std::map<FieldID, FieldDropoutStats> DropoutPresenter::getAllStats(
    NodeID node_id) const {
  return {};
}

int DropoutPresenter::applyDecisionToSimilar(
    NodeID node_id, FieldID field_id, const DetectedDropout& reference_dropout,
    DropoutDecision decision) {
  return 0;
}

int DropoutPresenter::autoDecideDropouts(NodeID node_id, FieldID field_id,
                                         double severity_threshold) {
  return 0;
}

bool DropoutPresenter::exportCorrections(NodeID node_id,
                                         const std::string& file_path) const {
  return false;
}

bool DropoutPresenter::importCorrections(NodeID node_id,
                                         const std::string& file_path) {
  return false;
}

std::vector<uint8_t> DropoutPresenter::getFieldData(
    const std::shared_ptr<void>& field_repr_handle, FieldID field_id,
    int& width, int& height) {
  width = 0;
  height = 0;

  auto artifact_base =
      std::static_pointer_cast<const orc::Artifact>(field_repr_handle);
  auto vfr = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      artifact_base);
  if (!vfr) {
    ORC_LOG_ERROR(
        "DropoutPresenter::getFieldData: no VideoFrameRepresentation found");
    return {};
  }

  try {
    // FieldID→FrameID: frame = field / 2; field index within frame: 1 or 2
    orc::FrameID frame_id = static_cast<orc::FrameID>(field_id.value() / 2);
    int32_t field_1based = static_cast<int32_t>(field_id.value() % 2) + 1;

    if (!vfr->has_frame(frame_id)) {
      ORC_LOG_WARN(
          "DropoutPresenter::getFieldData: frame {} not in representation",
          frame_id);
      return {};
    }

    auto desc = vfr->get_frame_descriptor(frame_id);
    if (!desc) {
      ORC_LOG_ERROR(
          "DropoutPresenter::getFieldData: no descriptor for frame {}",
          frame_id);
      return {};
    }

    size_t frame_height = desc->height;
    size_t samples_per_line = desc->samples_per_line_nominal;
    size_t field_1_height = frame_height / 2;
    size_t field_2_height = frame_height - field_1_height;
    size_t field_height_lines =
        (field_1based == 1) ? field_1_height : field_2_height;
    size_t first_line = (field_1based == 1) ? 0 : field_1_height;

    width = static_cast<int>(samples_per_line);
    height = static_cast<int>(field_height_lines);

    ORC_LOG_DEBUG(
        "DropoutPresenter::getFieldData: field {} (frame {}, field {}) "
        "dims {}x{}",
        field_id.value(), frame_id, field_1based, width, height);

    // Extract field lines and convert int16_t CVBS_U10_4FSC to 8-bit grayscale
    std::vector<uint8_t> grayscale;
    grayscale.reserve(field_height_lines * samples_per_line);

    bool use_luma = vfr->has_separate_channels();

    for (size_t line = first_line; line < first_line + field_height_lines;
         ++line) {
      const orc::VideoFrameRepresentation::sample_type* line_data =
          use_luma ? vfr->get_line_luma(frame_id, line)
                   : vfr->get_line(frame_id, line);
      if (!line_data) {
        // Fill with black for missing lines
        for (size_t s = 0; s < samples_per_line; ++s) {
          grayscale.push_back(0);
        }
        continue;
      }
      for (size_t s = 0; s < samples_per_line; ++s) {
        int16_t sample = line_data[s];
        // CVBS_U10_4FSC: 10-bit range 0-1023 → clamp and shift to 8-bit
        int32_t clamped = (sample < 0) ? 0 : (sample > 1023 ? 1023 : sample);
        grayscale.push_back(static_cast<uint8_t>(clamped >> 2));
      }
    }

    ORC_LOG_DEBUG(
        "DropoutPresenter::getFieldData: {} samples extracted for field {}",
        grayscale.size(), field_id.value());
    return grayscale;
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Error getting field data: {}", e.what());
    return {};
  }
}

std::vector<DropoutRegion> DropoutPresenter::getSourceDropouts(
    const std::shared_ptr<void>& field_repr_handle, FieldID field_id) {
  auto artifact_base =
      std::static_pointer_cast<const orc::Artifact>(field_repr_handle);
  auto vfr = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      artifact_base);
  if (!vfr) {
    return {};
  }

  try {
    orc::FrameID frame_id = static_cast<orc::FrameID>(field_id.value() / 2);
    int32_t field_1based = static_cast<int32_t>(field_id.value() % 2) + 1;

    if (!vfr->has_frame(frame_id)) {
      return {};
    }

    auto desc = vfr->get_frame_descriptor(frame_id);
    if (!desc) {
      return {};
    }

    auto vp = vfr->get_video_parameters();
    if (!vp) {
      return {};
    }

    // Get frame-flat dropout hints and convert to field-relative DropoutRegions
    auto runs = vfr->get_dropout_hints(frame_id);
    std::vector<DropoutRegion> regions;
    regions.reserve(runs.size());

    for (const auto& run : runs) {
      auto fls = orc::dropout_util::frame_sample_to_field_line(
          vp->system, run.sample_start);
      if (fls.field != field_1based) {
        continue;
      }

      // Convert run end (clamp to same line for simplicity)
      uint32_t end_sample =
          fls.sample + static_cast<uint32_t>(run.sample_count);
      uint32_t samples_per_line =
          static_cast<uint32_t>(desc->samples_per_line_nominal);
      if (end_sample > samples_per_line) {
        end_sample = samples_per_line;
      }

      DropoutRegion region;
      region.line = static_cast<uint32_t>(fls.line);
      region.start_sample = static_cast<uint32_t>(fls.sample);
      region.end_sample = end_sample;
      region.basis = DropoutRegion::DetectionBasis::HINT_DERIVED;
      regions.push_back(region);
    }

    return regions;
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Error getting source dropouts: {}", e.what());
    return {};
  }
}

std::map<uint64_t, FieldDropoutMap> DropoutPresenter::getDropoutMap(
    NodeID node_id) {
  try {
    // Find the node in the project
    const auto& nodes = impl_->project_presenter_.getNodes();
    auto node_it = std::find_if(nodes.begin(), nodes.end(),
                                [&node_id](const orc::presenters::NodeInfo& n) {
                                  return n.node_id == node_id;
                                });

    if (node_it == nodes.end()) {
      ORC_LOG_WARN("Node {} not found in project", node_id.to_string());
      return {};
    }

    // Check if it's a dropout_map stage
    if (node_it->stage_name != "dropout_map") {
      ORC_LOG_WARN("Node {} is not a dropout_map stage (it's {})",
                   node_id.to_string(), node_it->stage_name);
      return {};
    }

    // Get parameters
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
    NodeID node_id, const std::map<uint64_t, FieldDropoutMap>& dropout_map) {
  try {
    std::string map_str = encode_dropout_map_string(dropout_map);

    // Set parameter using ProjectPresenter
    std::map<std::string, orc::ParameterValue> params;
    params["dropout_map"] = map_str;

    impl_->project_presenter_.setNodeParameters(node_id, params);

    return true;
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Error setting dropout map: {}", e.what());
    return false;
  }
}

size_t DropoutPresenter::getFieldCount(
    const std::shared_ptr<void>& field_repr_handle) {
  auto artifact_base =
      std::static_pointer_cast<const orc::Artifact>(field_repr_handle);
  auto vfr = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      artifact_base);
  if (!vfr) {
    return 0;
  }
  return vfr->frame_count() * 2;
}

}  // namespace orc::presenters
