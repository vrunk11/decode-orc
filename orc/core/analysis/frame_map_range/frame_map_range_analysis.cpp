/*
 * File:        frame_map_range_analysis.cpp
 * Module:      analysis
 * Purpose:     Frame map range locator: finds frames by picture number or CLV
 * timecode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "frame_map_range_analysis.h"

#include <orc/stage/logging.h>
#include <orc/stage/observers/biphase_observer.h>
#include <orc/stage/video_frame_representation.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../../include/dag_executor.h"
#include "../../include/project.h"
#include "../analysis_registry.h"

namespace orc {

// Force linker to include this object file (for static registration)
void force_link_FrameMapRangeAnalysisTool() {}

// Register the tool
REGISTER_ANALYSIS_TOOL(FrameMapRangeAnalysisTool)

std::string FrameMapRangeAnalysisTool::id() const { return "frame_map_range"; }

std::string FrameMapRangeAnalysisTool::name() const {
  return "Frame Map Range Finder";
}

std::string FrameMapRangeAnalysisTool::description() const {
  return "Find a frame range by start/end picture number or CLV timecode and "
         "generate a Frame Map range specification.";
}

std::string FrameMapRangeAnalysisTool::category() const { return "Diagnostic"; }

std::vector<ParameterDescriptor> FrameMapRangeAnalysisTool::parameters() const {
  std::vector<ParameterDescriptor> params;

  ParameterDescriptor start_addr;
  start_addr.name = "startAddress";
  start_addr.display_name = "Start Address";
  start_addr.description =
      "Start picture number (e.g., '12345') or CLV timecode (e.g., '0:0:0.0').";
  start_addr.type = ParameterType::STRING;
  start_addr.constraints.default_value = std::string("");
  start_addr.constraints.required = true;
  params.push_back(start_addr);

  ParameterDescriptor end_addr;
  end_addr.name = "endAddress";
  end_addr.display_name = "End Address";
  end_addr.description =
      "End picture number (e.g., '12350') or CLV timecode (e.g., '0:0:0.5').";
  end_addr.type = ParameterType::STRING;
  end_addr.constraints.default_value = std::string("");
  end_addr.constraints.required = true;
  params.push_back(end_addr);

  return params;
}

bool FrameMapRangeAnalysisTool::canAnalyze(
    AnalysisSourceType source_type) const {
  return source_type == AnalysisSourceType::LaserDisc;
}

bool FrameMapRangeAnalysisTool::isApplicableToStage(
    const std::string& stage_name) const {
  return stage_name == "frame_map";
}

namespace {

struct ParsedAddress {
  bool ok = false;
  bool is_timecode = false;
  int32_t picture_number = 0;
  std::string normalized;
  std::string error;
};

static std::string trim_copy(const std::string& input) {
  size_t start = 0;
  while (start < input.size() &&
         std::isspace(static_cast<unsigned char>(input[start]))) {
    start++;
  }
  size_t end = input.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    end--;
  }
  return input.substr(start, end - start);
}

static bool parse_int32(const std::string& input, int32_t& value) {
  try {
    size_t pos = 0;
    int64_t parsed = std::stoll(input, &pos, 10);
    if (pos != input.size()) {
      return false;
    }
    if (parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max()) {
      return false;
    }
    value = static_cast<int32_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

static ParsedAddress parse_address(const std::string& input, bool is_pal) {
  ParsedAddress result;
  std::string trimmed = trim_copy(input);
  if (trimmed.empty()) {
    result.error = "Address is empty";
    return result;
  }

  if (trimmed.find(':') == std::string::npos) {
    int32_t pn = 0;
    if (!parse_int32(trimmed, pn) || pn <= 0) {
      result.error = "Invalid picture number: " + trimmed;
      return result;
    }
    result.ok = true;
    result.is_timecode = false;
    result.picture_number = pn;
    result.normalized = trimmed;
    return result;
  }

  std::vector<std::string> parts;
  std::stringstream ss(trimmed);
  std::string part;
  while (std::getline(ss, part, ':')) {
    parts.push_back(trim_copy(part));
  }

  if (parts.size() < 3 || parts.size() > 4) {
    result.error = "Invalid timecode format: " + trimmed;
    return result;
  }

  int32_t hours = 0;
  int32_t minutes = 0;
  int32_t seconds = 0;
  int32_t pictures = 0;

  if (!parse_int32(parts[0], hours) || !parse_int32(parts[1], minutes)) {
    result.error = "Invalid timecode format: " + trimmed;
    return result;
  }

  if (parts.size() == 4) {
    if (!parse_int32(parts[2], seconds) || !parse_int32(parts[3], pictures)) {
      result.error = "Invalid timecode format: " + trimmed;
      return result;
    }
  } else {
    const std::string& sec_part = parts[2];
    size_t dot_pos = sec_part.find('.');
    if (dot_pos == std::string::npos) {
      dot_pos = sec_part.find(';');
    }
    if (dot_pos == std::string::npos) {
      result.error = "Timecode must include picture component (e.g., 0:0:0.0)";
      return result;
    }
    std::string seconds_str = trim_copy(sec_part.substr(0, dot_pos));
    std::string pictures_str = trim_copy(sec_part.substr(dot_pos + 1));
    if (!parse_int32(seconds_str, seconds) ||
        !parse_int32(pictures_str, pictures)) {
      result.error = "Invalid timecode format: " + trimmed;
      return result;
    }
  }

  int32_t fps = is_pal ? 25 : 30;
  int64_t frame_index = static_cast<int64_t>(hours) * 3600LL * fps +
                        static_cast<int64_t>(minutes) * 60LL * fps +
                        static_cast<int64_t>(seconds) * fps +
                        static_cast<int64_t>(pictures);
  int64_t picture_number = frame_index + 1;  // 0:0:0.0 = picture number 1
  if (picture_number <= 0 ||
      picture_number > std::numeric_limits<int32_t>::max()) {
    result.error = "Timecode out of range: " + trimmed;
    return result;
  }

  std::ostringstream normalized;
  normalized << hours << ":" << minutes << ":" << seconds << "." << pictures;
  result.ok = true;
  result.is_timecode = true;
  result.picture_number = static_cast<int32_t>(picture_number);
  result.normalized = normalized.str();
  return result;
}

// Read picture number from one field slot in the observation context.
static std::optional<int32_t> get_picture_number_from_vbi(
    const ObservationContext& observation_context, FieldID field_id,
    bool is_pal) {
  auto picture_num = observation_context.get(field_id, "vbi", "picture_number");
  if (picture_num && std::holds_alternative<int32_t>(*picture_num)) {
    return std::get<int32_t>(*picture_num);
  }

  auto hours_opt =
      observation_context.get(field_id, "vbi", "clv_timecode_hours");
  auto minutes_opt =
      observation_context.get(field_id, "vbi", "clv_timecode_minutes");
  auto seconds_opt =
      observation_context.get(field_id, "vbi", "clv_timecode_seconds");
  auto picture_opt =
      observation_context.get(field_id, "vbi", "clv_timecode_picture");

  if (hours_opt && minutes_opt && seconds_opt && picture_opt) {
    if (std::holds_alternative<int32_t>(*hours_opt) &&
        std::holds_alternative<int32_t>(*minutes_opt) &&
        std::holds_alternative<int32_t>(*seconds_opt) &&
        std::holds_alternative<int32_t>(*picture_opt)) {
      int32_t hours = std::get<int32_t>(*hours_opt);
      int32_t minutes = std::get<int32_t>(*minutes_opt);
      int32_t seconds = std::get<int32_t>(*seconds_opt);
      int32_t picture = std::get<int32_t>(*picture_opt);
      int32_t fps = is_pal ? 25 : 30;
      int64_t frame_index = static_cast<int64_t>(hours) * 3600LL * fps +
                            static_cast<int64_t>(minutes) * 60LL * fps +
                            static_cast<int64_t>(seconds) * fps +
                            static_cast<int64_t>(picture);
      int64_t picture_number = frame_index + 1;  // 0:0:0.0 = picture number 1
      if (picture_number > 0 &&
          picture_number <= std::numeric_limits<int32_t>::max()) {
        return static_cast<int32_t>(picture_number);
      }
    }
  }

  return std::nullopt;
}

// BiphaseObserver stores VBI per-field at FieldID(frame_id*2 + field_idx).
// Check both fields and return the first valid picture number found.
static std::optional<int32_t> get_picture_number_from_frame(
    const ObservationContext& obs_context, FrameID frame_id, bool is_pal) {
  for (uint64_t field_idx = 0; field_idx < 2; ++field_idx) {
    auto pn = get_picture_number_from_vbi(
        obs_context, FieldID(frame_id * 2 + field_idx), is_pal);
    if (pn) return pn;
  }
  return std::nullopt;
}

}  // namespace

AnalysisResult FrameMapRangeAnalysisTool::analyze(const AnalysisContext& ctx,
                                                  AnalysisProgress* progress) {
  AnalysisResult result;

  if (progress) {
    progress->setStatus("Initializing frame map range analysis...");
    progress->setProgress(0);
  }

  std::string start_input;
  std::string end_input;

  auto param_it = ctx.parameters.find("startAddress");
  if (param_it != ctx.parameters.end() &&
      std::holds_alternative<std::string>(param_it->second)) {
    start_input = std::get<std::string>(param_it->second);
  }
  param_it = ctx.parameters.find("endAddress");
  if (param_it != ctx.parameters.end() &&
      std::holds_alternative<std::string>(param_it->second)) {
    end_input = std::get<std::string>(param_it->second);
  }

  if (start_input.empty() || end_input.empty()) {
    result.status = AnalysisResult::Failed;
    result.summary = "Start and end addresses are required.";
    return result;
  }

  if (!ctx.dag || !ctx.project) {
    result.status = AnalysisResult::Failed;
    result.summary = "No DAG or project provided for analysis";
    ORC_LOG_ERROR(
        "Frame map range analysis requires DAG and project in context");
    return result;
  }

  const auto& dag_nodes = ctx.dag->nodes();
  auto node_it = std::find_if(
      dag_nodes.begin(), dag_nodes.end(),
      [&ctx](const DAGNode& node) { return node.node_id == ctx.node_id; });

  if (node_it == dag_nodes.end()) {
    result.status = AnalysisResult::Failed;
    result.summary = "Node not found in DAG";
    ORC_LOG_ERROR("Node '{}' not found in DAG", ctx.node_id);
    return result;
  }

  if (node_it->input_node_ids.empty()) {
    result.status = AnalysisResult::Failed;
    result.summary = "Frame map node has no input connected";
    ORC_LOG_ERROR("Frame map node '{}' has no input", ctx.node_id);
    return result;
  }

  NodeID input_node_id = node_it->input_node_ids[0];
  ORC_LOG_DEBUG(
      "Node '{}': Frame map range analysis - getting input from node '{}'",
      ctx.node_id, input_node_id);

  DAGExecutor executor;
  std::shared_ptr<VideoFrameRepresentation> source;

  try {
    auto all_outputs = executor.execute_to_node(*ctx.dag, input_node_id);
    auto output_it = all_outputs.find(input_node_id);
    if (output_it == all_outputs.end() || output_it->second.empty()) {
      result.status = AnalysisResult::Failed;
      result.summary = "Input node produced no outputs";
      ORC_LOG_ERROR("Node '{}': Input node '{}' produced no outputs",
                    ctx.node_id, input_node_id);
      return result;
    }

    for (const auto& artifact : output_it->second) {
      source = std::dynamic_pointer_cast<VideoFrameRepresentation>(artifact);
      if (source) {
        break;
      }
    }

    if (!source) {
      result.status = AnalysisResult::Failed;
      result.summary = "Input node did not produce VideoFrameRepresentation";
      ORC_LOG_ERROR(
          "Node '{}': Input node '{}' did not produce VideoFrameRepresentation",
          ctx.node_id, input_node_id);
      return result;
    }
  } catch (const std::exception& e) {
    result.status = AnalysisResult::Failed;
    result.summary = "Analysis failed: " + std::string(e.what());
    ORC_LOG_ERROR("Frame map range analysis failed: {}", e.what());
    return result;
  }

  auto frame_range = source->frame_range();
  size_t total_frames = frame_range.count();

  if (total_frames == 0) {
    result.status = AnalysisResult::Failed;
    result.summary = "No frames found in source";
    return result;
  }

  bool is_pal = false;
  auto first_frame_desc = source->get_frame_descriptor(frame_range.first);
  if (first_frame_desc) {
    is_pal = (first_frame_desc->system == VideoSystem::PAL);
  }

  ParsedAddress start_addr = parse_address(start_input, is_pal);
  if (!start_addr.ok) {
    result.status = AnalysisResult::Failed;
    result.summary = "Start address error: " + start_addr.error;
    return result;
  }

  ParsedAddress end_addr = parse_address(end_input, is_pal);
  if (!end_addr.ok) {
    result.status = AnalysisResult::Failed;
    result.summary = "End address error: " + end_addr.error;
    return result;
  }

  if (progress) {
    progress->setStatus("Finding first valid VBI to establish baseline...");
    progress->setProgress(10);
  }

  BiphaseObserver biphase_observer;
  auto& obs_context = executor.get_observation_context();
  std::set<FrameID> processed_frames;

  auto extract_vbi_if_needed = [&](FrameID frame_id) {
    if (processed_frames.find(frame_id) == processed_frames.end()) {
      biphase_observer.process_frame(*source, frame_id, obs_context);
      processed_frames.insert(frame_id);
    }
  };

  // Find the first frame with valid VBI to establish a prediction baseline
  std::optional<int32_t> first_picture_number;
  FrameID first_valid_frame = frame_range.first;

  for (FrameID fid = frame_range.first; fid <= frame_range.last; ++fid) {
    extract_vbi_if_needed(fid);
    auto pn_opt = get_picture_number_from_frame(obs_context, fid, is_pal);
    if (pn_opt) {
      first_picture_number = pn_opt;
      first_valid_frame = fid;
      ORC_LOG_DEBUG("First valid VBI at frame {}: picture number {}", fid,
                    *pn_opt);
      break;
    }
  }

  if (!first_picture_number) {
    result.status = AnalysisResult::Failed;
    result.summary =
        "No valid VBI data found in source. Cannot locate picture "
        "numbers/timecodes.";
    return result;
  }

  if (progress) {
    progress->setStatus("Analyzing picture-to-frame mapping...");
    progress->setProgress(20);
  }

  // Sample spread points to measure the frames-per-picture ratio (handles gaps
  // and non-uniform spacing; expected ~1.0 for normal CAV/CLV discs)
  std::vector<std::pair<int64_t, int32_t>> samples;  // (frame_id, picture_num)
  samples.push_back(
      {static_cast<int64_t>(first_valid_frame), first_picture_number.value()});

  const size_t max_samples = 11;
  size_t sample_interval =
      std::max<size_t>(1, total_frames / (max_samples * 10));

  for (size_t i = 1; i < max_samples && samples.size() < max_samples; i++) {
    FrameID sample_frame = first_valid_frame + (i * sample_interval);
    if (sample_frame > frame_range.last) break;
    extract_vbi_if_needed(sample_frame);
    auto pn_opt =
        get_picture_number_from_frame(obs_context, sample_frame, is_pal);
    if (pn_opt) {
      samples.push_back({static_cast<int64_t>(sample_frame), *pn_opt});
    }
  }

  double avg_frames_per_picture = 1.0;  // default fallback
  if (samples.size() > 1) {
    int64_t total_frame_delta = 0;
    int64_t total_picture_delta = 0;
    for (size_t i = 1; i < samples.size(); i++) {
      int64_t frame_delta = samples[i].first - samples[i - 1].first;
      int64_t picture_delta = samples[i].second - samples[i - 1].second;
      if (picture_delta > 0 && frame_delta > 0) {
        total_frame_delta += frame_delta;
        total_picture_delta += picture_delta;
      }
    }
    if (total_picture_delta > 0) {
      avg_frames_per_picture = static_cast<double>(total_frame_delta) /
                               static_cast<double>(total_picture_delta);
    }
  }

  ORC_LOG_DEBUG("Sampled {} points, calculated avg frames per picture: {:.2f}",
                samples.size(), avg_frames_per_picture);

  int64_t start_picture_offset =
      start_addr.picture_number - first_picture_number.value();
  int64_t end_picture_offset =
      end_addr.picture_number - first_picture_number.value();

  int64_t src_first = static_cast<int64_t>(frame_range.first);
  int64_t src_last = static_cast<int64_t>(frame_range.last);

  int64_t predicted_start_frame =
      static_cast<int64_t>(first_valid_frame) +
      static_cast<int64_t>(static_cast<double>(start_picture_offset) *
                           avg_frames_per_picture);
  int64_t predicted_end_frame =
      static_cast<int64_t>(first_valid_frame) +
      static_cast<int64_t>(static_cast<double>(end_picture_offset) *
                           avg_frames_per_picture);

  predicted_start_frame =
      std::max(src_first, std::min(predicted_start_frame, src_last));
  predicted_end_frame =
      std::max(src_first, std::min(predicted_end_frame, src_last));

  ORC_LOG_DEBUG(
      "Predicted start frame: {} (picture {}), predicted end frame: {} "
      "(picture {})",
      predicted_start_frame, start_addr.picture_number, predicted_end_frame,
      end_addr.picture_number);

  if (progress) {
    progress->setStatus("Jumping to predicted start position...");
    progress->setProgress(40);
  }

  bool start_found = false;
  FrameID start_frame = frame_range.first;

  // ±2500 frames (~100 s at 25 fps) around the predicted position
  const int64_t search_radius = 2500;
  int64_t start_search_begin =
      std::max(src_first, predicted_start_frame - search_radius);
  int64_t start_search_end =
      std::min(src_last + 1, predicted_start_frame + search_radius);

  ORC_LOG_DEBUG(
      "Searching for start picture {} in frame range {}-{} (predicted: {})",
      start_addr.picture_number, start_search_begin, start_search_end,
      predicted_start_frame);

  for (int64_t fid = start_search_begin; fid < start_search_end; ++fid) {
    FrameID frame_id = static_cast<FrameID>(fid);
    extract_vbi_if_needed(frame_id);
    auto pn_opt = get_picture_number_from_frame(obs_context, frame_id, is_pal);
    if (pn_opt && *pn_opt == start_addr.picture_number) {
      start_found = true;
      start_frame = frame_id;
      ORC_LOG_DEBUG("Start position found at frame {}: picture number {}", fid,
                    *pn_opt);
      break;
    }
  }

  // Fallback: full scan from the beginning
  if (!start_found) {
    ORC_LOG_WARN(
        "Start not found in predicted range, falling back to full scan");
    if (progress) {
      progress->setStatus(
          "Start not in predicted range, scanning from beginning...");
    }

    for (FrameID fid = frame_range.first; fid <= frame_range.last; ++fid) {
      extract_vbi_if_needed(fid);
      auto pn_opt = get_picture_number_from_frame(obs_context, fid, is_pal);
      if (pn_opt && *pn_opt == start_addr.picture_number) {
        start_found = true;
        start_frame = fid;
        ORC_LOG_DEBUG(
            "Start position found at frame {} (full scan): picture number {}",
            fid, *pn_opt);
        break;
      }

      if (progress && (fid % 2500 == 0)) {
        progress->setProgress(
            50 + static_cast<int>(15.0 *
                                  static_cast<double>(fid - frame_range.first) /
                                  static_cast<double>(total_frames)));
        if (progress->isCancelled()) {
          AnalysisResult cancelled_result;
          cancelled_result.status = AnalysisResult::Cancelled;
          return cancelled_result;
        }
      }
    }
  }

  if (!start_found) {
    result.status = AnalysisResult::Failed;
    result.summary = "Start address not found in source.";
    return result;
  }

  if (progress) {
    progress->setStatus("Searching for end position...");
    progress->setProgress(70);
    if (progress->isCancelled()) {
      result.status = AnalysisResult::Cancelled;
      return result;
    }
  }

  bool end_found = false;
  FrameID end_frame = start_frame;

  if (start_addr.picture_number == end_addr.picture_number) {
    end_found = true;
    ORC_LOG_DEBUG("End position (same as start) at frame {}: picture number {}",
                  end_frame, start_addr.picture_number);
  } else {
    int64_t end_search_begin = std::max(static_cast<int64_t>(start_frame),
                                        predicted_end_frame - search_radius);
    int64_t end_search_end =
        std::min(src_last + 1, predicted_end_frame + search_radius);

    ORC_LOG_DEBUG(
        "Searching for end picture {} in frame range {}-{} (predicted: {})",
        end_addr.picture_number, end_search_begin, end_search_end,
        predicted_end_frame);

    std::optional<FrameID> last_matching_frame;
    for (int64_t fid = end_search_begin; fid < end_search_end; ++fid) {
      FrameID frame_id = static_cast<FrameID>(fid);
      extract_vbi_if_needed(frame_id);
      auto pn_opt =
          get_picture_number_from_frame(obs_context, frame_id, is_pal);
      if (pn_opt && *pn_opt == end_addr.picture_number) {
        last_matching_frame = frame_id;
      }
    }

    if (last_matching_frame) {
      end_found = true;
      end_frame = *last_matching_frame;
      ORC_LOG_DEBUG("End position found at frame {}: picture number {}",
                    end_frame, end_addr.picture_number);
    }

    // Fallback: scan forward from start
    if (!end_found) {
      ORC_LOG_WARN(
          "End not found in predicted range, falling back to scan from start "
          "position");
      if (progress) {
        progress->setStatus(
            "End not in predicted range, scanning from start...");
      }

      for (FrameID fid = start_frame + 1; fid <= frame_range.last; ++fid) {
        extract_vbi_if_needed(fid);
        auto pn_opt = get_picture_number_from_frame(obs_context, fid, is_pal);
        if (pn_opt && *pn_opt == end_addr.picture_number) {
          end_found = true;
          end_frame = fid;
        } else if (end_found && pn_opt && *pn_opt != end_addr.picture_number) {
          break;
        }

        if (progress && (fid % 2500 == 0)) {
          FrameID frames_from_start = fid - start_frame;
          FrameID total_to_scan = frame_range.last - start_frame;
          progress->setProgress(
              70 +
              static_cast<int>(
                  20.0 * static_cast<double>(frames_from_start) /
                  static_cast<double>(std::max<FrameID>(1, total_to_scan))));
          if (progress->isCancelled()) {
            result.status = AnalysisResult::Cancelled;
            return result;
          }
        }
      }

      if (end_found) {
        ORC_LOG_DEBUG(
            "End position found at frame {} (full scan): picture number {}",
            end_frame, end_addr.picture_number);
      }
    }
  }

  if (progress) {
    progress->setProgress(90);
    if (progress->isCancelled()) {
      result.status = AnalysisResult::Cancelled;
      return result;
    }
  }

  if (!end_found) {
    result.status = AnalysisResult::Failed;
    result.summary = "End address not found after start address.";
    return result;
  }

  if (start_frame > end_frame) {
    result.status = AnalysisResult::Failed;
    result.summary = "Computed frame range is invalid (start after end).";
    return result;
  }

  std::ostringstream range_spec;
  range_spec << start_frame << "-" << end_frame;

  result.graphData["rangeSpec"] = range_spec.str();

  std::ostringstream summary;
  summary << "Frame range located successfully.\n\n";
  summary << "Start address: " << start_input << " (picture number "
          << start_addr.picture_number << ")\n";
  summary << "End address: " << end_input << " (picture number "
          << end_addr.picture_number << ")\n\n";
  summary << "Frame range: " << start_frame << "-" << end_frame << "\n";
  summary << "Range spec: " << range_spec.str() << "\n\n";
  summary << "Click 'Apply to Stage' to update the Frame Map stage.";

  result.summary = summary.str();
  result.status = AnalysisResult::Success;

  if (progress) {
    progress->setStatus("Analysis complete");
    progress->setProgress(100);
  }

  return result;
}

bool FrameMapRangeAnalysisTool::canApplyToGraph() const { return true; }

bool FrameMapRangeAnalysisTool::applyToGraph(AnalysisResult& result,
                                             const Project& /*project*/,
                                             [[maybe_unused]] NodeID node_id) {
  if (result.status != AnalysisResult::Success) {
    ORC_LOG_ERROR("Cannot apply failed analysis result");
    return false;
  }

  auto it = result.graphData.find("rangeSpec");
  if (it == result.graphData.end()) {
    ORC_LOG_ERROR("Analysis result does not contain rangeSpec");
    return false;
  }

  const std::string& range_spec = it->second;

  try {
    result.parameterChanges["ranges"] = range_spec;
    ORC_LOG_DEBUG("Prepared range spec '{}' for node '{}'", range_spec,
                  node_id);
    return true;
  } catch (const std::exception& e) {
    ORC_LOG_ERROR("Failed to prepare range spec: {}", e.what());
    return false;
  }
}

int FrameMapRangeAnalysisTool::estimateDurationSeconds(
    const AnalysisContext& ctx) const {
  (void)ctx;
  return 10;
}

}  // namespace orc
