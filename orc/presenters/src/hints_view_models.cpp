/*
 * File:        hints_view_models.cpp
 * Module:      orc-presenters
 * Purpose:     Implementation of hint view model conversion functions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "../include/hints_view_models.h"

#include <common_types.h>  // For VideoSystem enum
#include <cvbs_signal_constants.h>
#include <orc_source_parameters.h>

namespace orc::presenters {

VideoParametersView toVideoParametersView(const orc::SourceParameters& params) {
  VideoParametersView v{};

  // Convert VideoSystem enum
  switch (params.system) {
    case orc::VideoSystem::PAL:
      v.system = VideoSystem::PAL;
      break;
    case orc::VideoSystem::NTSC:
      v.system = VideoSystem::NTSC;
      break;
    case orc::VideoSystem::PAL_M:
      v.system = VideoSystem::PAL_M;
      break;
    default:
      v.system = VideoSystem::Unknown;
      break;
  }

  // Canonical CVBS_U10_4FSC fields (not deprecated).
  v.active_video_start = params.active_video_start;
  v.active_video_end = params.active_video_end;

  // Presenter view model fields — derived from the video system.
  // EBU Tech. 3280-E §1.1 (PAL) / SMPTE 244M-2003 §4.1 (NTSC) /
  // ITU-R BT.1700-1 Annex 1 Part B (PAL_M).
  v.frame_width_nominal = params.frame_width_nominal;
  const auto [cb_start, cb_end] = colour_burst_range(params.system);
  v.color_burst_start = cb_start;
  v.color_burst_end = cb_end;

  // CVBS_U10_4FSC 10-bit domain signal levels
  v.sync_tip_level = params.sync_tip_level;
  v.blanking_level = params.blanking_level;
  v.black_level = params.black_level;
  v.white_level = params.white_level;
  v.peak_level = params.peak_level;
  v.chroma_dc_offset = params.chroma_dc_offset;

  return v;
}

std::optional<FieldToFrameDisplayResult> fieldToFrameCoordinates(
    VideoSystem system, uint64_t field_index, int field_line_number) {
  if (field_line_number < 1) {
    return std::nullopt;  // Invalid line number
  }

  // Determine if this is the first or second field
  bool is_first_field = (field_index % 2) == 0;

  // Calculate frame number (1-based)
  uint64_t frame_number = (field_index / 2) + 1;

  // Get field heights based on system and parity
  size_t first_field_height = 0;
  switch (system) {
    case VideoSystem::NTSC:
    case VideoSystem::PAL_M:
      // NTSC: First field = 262 lines, Second field = 263 lines
      first_field_height = 262;
      break;
    case VideoSystem::PAL:
      // PAL: First field = 312 lines, Second field = 313 lines
      first_field_height = 312;
      break;
    case VideoSystem::Unknown:
    default:
      return std::nullopt;  // Unknown system
  }

  // Calculate frame line number based on field parity
  int frame_line_number;
  if (is_first_field) {
    // First field: frame lines start at 1
    frame_line_number = field_line_number;
  } else {
    // Second field: frame lines are offset by first field height
    frame_line_number =
        field_line_number + static_cast<int>(first_field_height);
  }

  return FieldToFrameDisplayResult{frame_number, frame_line_number,
                                   is_first_field};
}

}  // namespace orc::presenters
