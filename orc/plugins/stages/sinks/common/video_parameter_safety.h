/*
 * File:        video_parameter_safety.h
 * Module:      orc-core
 * Purpose:     Shared validation and sanitisation for chroma sink video
 * parameters
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ORC_CORE_CHROMA_SINK_VIDEO_PARAMETER_SAFETY_H
#define ORC_CORE_CHROMA_SINK_VIDEO_PARAMETER_SAFETY_H

#include <orc/stage/cvbs_signal_constants.h>
#include <orc/stage/orc_source_parameters.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace orc::chroma_sink {

enum class DecoderVideoProfile { Mono, NtscColour, PalColour };

struct VideoParameterSafetyResult {
  bool ok = true;
  SourceParameters params;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
};

// Return the full-frame height derived from the video system's padded field
// height.  Used by decoders that need to know the frame dimensions without
// relying on the deprecated field_height struct member.
inline int32_t get_frame_height(const SourceParameters& params) {
  const size_t fh = calculate_padded_field_height(params.system);
  if (fh == 0) {
    return -1;
  }
  return static_cast<int32_t>(fh) * 2 - 1;
}

inline std::string join_issues(const std::vector<std::string>& issues) {
  std::ostringstream stream;

  for (size_t index = 0; index < issues.size(); ++index) {
    if (index > 0) {
      stream << "; ";
    }
    stream << issues[index];
  }

  return stream.str();
}

inline VideoParameterSafetyResult sanitize_video_parameters(
    const SourceParameters& input, DecoderVideoProfile profile) {
  VideoParameterSafetyResult result;
  result.params = input;

  auto add_warning = [&result](const std::string& message) {
    result.warnings.push_back(message);
  };
  auto add_error = [&result](const std::string& message) {
    result.ok = false;
    result.errors.push_back(message);
  };

  auto& params = result.params;

  if (params.frame_width_nominal <= 0) {
    add_error("frame_width_nominal must be greater than 0");
    return result;
  }

  if (params.system == VideoSystem::Unknown) {
    add_error("video system must be known for colour decoding");
    return result;
  }

  constexpr int32_t kGenericMaxWidth = 1135;
  constexpr int32_t kGenericMaxFrameHeight = 625;
  constexpr int32_t kNtscDecoderMinStart = 16;
  constexpr int32_t kNtscDecoderRightMargin = 2;
  constexpr int32_t kPalFilterMargin = 7;

  int32_t max_width = kGenericMaxWidth;
  int32_t max_frame_height = kGenericMaxFrameHeight;
  int32_t min_active_start = 0;
  int32_t right_margin = 0;
  const bool requires_colour_reference = (profile != DecoderVideoProfile::Mono);

  if (profile == DecoderVideoProfile::NtscColour) {
    max_width = 910;
    max_frame_height = 625;
    min_active_start = kNtscDecoderMinStart;
    right_margin = kNtscDecoderRightMargin;
  } else if (profile == DecoderVideoProfile::PalColour) {
    max_width = 1135;
    max_frame_height = 625;
    min_active_start = kPalFilterMargin;
    right_margin = kPalFilterMargin;
  }

  if (params.frame_width_nominal > max_width) {
    add_warning(
        "frame_width_nominal exceeds decoder limit and will be clamped");
    params.frame_width_nominal = max_width;
  }

  const int32_t frame_height = get_frame_height(params);
  if (frame_height <= 0) {
    add_error("video system produces an invalid frame height");
    return result;
  }

  if (frame_height > max_frame_height) {
    add_warning(
        "video system frame height exceeds decoder limit; check system "
        "setting");
  }

  const int32_t max_active_end = params.frame_width_nominal - right_margin;
  if (max_active_end <= min_active_start) {
    add_error("frame_width_nominal is too small for the selected decoder");
    return result;
  }

  if (params.active_video_start < min_active_start ||
      params.active_video_start >= max_active_end) {
    add_warning("active_video_start is out of range and will be adjusted");
  }
  if (params.active_video_end <= min_active_start ||
      params.active_video_end > max_active_end) {
    add_warning("active_video_end is out of range and will be adjusted");
  }

  params.active_video_start = std::clamp(params.active_video_start,
                                         min_active_start, max_active_end - 1);
  params.active_video_end =
      std::clamp(params.active_video_end, min_active_start + 1, max_active_end);
  if (params.active_video_start >= params.active_video_end) {
    add_warning(
        "active video range is inverted and will be reset to decoder-safe "
        "bounds");
    params.active_video_start = min_active_start;
    params.active_video_end = max_active_end;
  }

  if (params.first_active_frame_line < 0 ||
      params.first_active_frame_line >= frame_height) {
    add_warning("first_active_frame_line is out of range and will be adjusted");
  }
  if (params.last_active_frame_line <= 0 ||
      params.last_active_frame_line > frame_height) {
    add_warning("last_active_frame_line is out of range and will be adjusted");
  }

  params.first_active_frame_line =
      std::clamp(params.first_active_frame_line, 0, frame_height - 1);
  params.last_active_frame_line =
      std::clamp(params.last_active_frame_line, 1, frame_height);
  if (params.first_active_frame_line >= params.last_active_frame_line) {
    add_warning(
        "active frame line range is inverted and will be reset to the full "
        "frame");
    params.first_active_frame_line = 0;
    params.last_active_frame_line = frame_height;
  }

  if (requires_colour_reference) {
    // sample_rate and fsc are now derived from the video system; always valid.
    // No validation needed for per-disc values.

    // Colour burst range is derived from the video system constant.
    // Nothing to clamp — the constants are always in range.
  }

  return result;
}

}  // namespace orc::chroma_sink

#endif  // ORC_CORE_CHROMA_SINK_VIDEO_PARAMETER_SAFETY_H
