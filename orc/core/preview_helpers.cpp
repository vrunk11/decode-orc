/*
 * File:        preview_helpers.cpp
 * Module:      orc-core
 * Purpose:     Helper functions for implementing PreviewableStage
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "preview_helpers.h"

#include <cvbs_signal_constants.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "logging.h"

namespace orc {
namespace PreviewHelpers {

// Scale a 10-bit CVBS_U10_4FSC int16_t sample to an 8-bit grayscale value.
// Clamped mode maps [black_level, white_level] → [0, 255].
// Raw mode maps [sync_tip_level (-300 mV), peak_level (1000 mV)] → [0, 255].
inline uint8_t scale_10bit_to_8bit(int16_t sample, bool apply_level_scaling,
                                   int32_t black_level, int32_t white_level,
                                   int32_t sync_tip_level, int32_t peak_level) {
  if (apply_level_scaling) {
    int32_t range = white_level - black_level;
    if (range <= 0) return 0;
    int32_t scaled =
        ((static_cast<int32_t>(sample) - black_level) * 255) / range;
    return static_cast<uint8_t>(std::max(0, std::min(255, scaled)));
  } else {
    // Raw: map [sync_tip_level, peak_level] → [0, 255] so the full analogue
    // signal range (-300 mV to 1000 mV) spans the display range.
    int32_t range = peak_level - sync_tip_level;
    if (range <= 0) return 0;
    int32_t scaled =
        ((static_cast<int32_t>(sample) - sync_tip_level) * 255) / range;
    return static_cast<uint8_t>(std::max(0, std::min(255, scaled)));
  }
}

std::vector<PreviewOption> get_standard_preview_options(
    const std::shared_ptr<const VideoFrameRepresentation>& representation) {
  std::vector<PreviewOption> options;

  if (!representation) {
    return options;
  }

  auto video_params = representation->get_video_parameters();
  if (!video_params || !video_params->is_valid()) {
    return options;
  }

  size_t frame_count = representation->frame_count();
  if (frame_count == 0) {
    return options;
  }

  uint32_t width = static_cast<uint32_t>(video_params->frame_width_nominal);
  uint32_t height = static_cast<uint32_t>(video_params->frame_height);

  // 1.0 is a neutral fallback; only the computed path reaches stages that
  // properly configure active area parameters.
  double dar_correction = 1.0;
  if (video_params->active_video_start >= 0 &&
      video_params->active_video_end > video_params->active_video_start &&
      video_params->first_active_frame_line >= 0 &&
      video_params->last_active_frame_line >
          video_params->first_active_frame_line) {
    uint32_t active_width = static_cast<uint32_t>(
        video_params->active_video_end - video_params->active_video_start);
    uint32_t active_height =
        static_cast<uint32_t>(video_params->last_active_frame_line -
                              video_params->first_active_frame_line);
    double active_ratio =
        static_cast<double>(active_width) / static_cast<double>(active_height);
    dar_correction = (4.0 / 3.0) / active_ratio;
  }

  options.push_back(PreviewOption{"interlaced_clamped", "Interlaced Clamped",
                                  false, width, height, frame_count,
                                  dar_correction});
  options.push_back(PreviewOption{"interlaced_raw", "Interlaced Raw", false,
                                  width, height, frame_count, dar_correction});
  options.push_back(PreviewOption{"sequential_clamped", "Sequential Clamped",
                                  false, width, height, frame_count,
                                  dar_correction});
  options.push_back(PreviewOption{"sequential_raw", "Sequential Raw", false,
                                  width, height, frame_count, dar_correction});

  return options;
}

PreviewImage render_standard_preview(
    const std::shared_ptr<const VideoFrameRepresentation>& representation,
    const std::string& option_id, uint64_t index, PreviewNavigationHint hint) {
  (void)hint;
  PreviewImage result{};

  if (!representation) {
    return result;
  }

  auto video_params = representation->get_video_parameters();
  if (!video_params || !video_params->is_valid()) {
    return result;
  }

  FrameID frame_id = representation->frame_range().first + index;
  if (!representation->has_frame(frame_id)) {
    return result;
  }

  auto descriptor = representation->get_frame_descriptor(frame_id);
  if (!descriptor) {
    return result;
  }

  const bool apply_level_scaling =
      (option_id == "sequential_clamped" || option_id == "interlaced_clamped");
  const bool do_interlace =
      (option_id == "interlaced_clamped" || option_id == "interlaced_raw");

  if (option_id != "sequential_clamped" && option_id != "sequential_raw" &&
      option_id != "interlaced_clamped" && option_id != "interlaced_raw") {
    ORC_LOG_WARN("PreviewHelpers (frame): Unknown preview option '{}'",
                 option_id);
    return result;
  }

  uint32_t width = static_cast<uint32_t>(descriptor->samples_per_line_nominal);
  uint32_t height = static_cast<uint32_t>(descriptor->height);
  int32_t black_level = video_params->black_level;
  int32_t white_level = video_params->white_level;
  int32_t sync_tip_level = video_params->sync_tip_level;
  int32_t peak_level = video_params->peak_level;

  // Determine field-line count and dominance for interlaced weaving.
  // VFR field 1 is always the top spatial field; all systems use even display
  // rows.
  //   PAL:   field 1 (313 lines, top) → even display rows.
  //   NTSC:  field 1 (263 lines, top) → even display rows.
  //   PAL_M: field 1 (263 lines, top) → even display rows.
  size_t field1_lines = static_cast<size_t>(height) / 2;
  bool field1_on_even_rows = true;
  if (do_interlace) {
    switch (video_params->system) {
      case VideoSystem::PAL:
        field1_lines = static_cast<size_t>(kPalField1Lines);
        field1_on_even_rows = true;
        break;
      case VideoSystem::NTSC:
        field1_lines = static_cast<size_t>(kNtscField1Lines);
        field1_on_even_rows = true;
        break;
      case VideoSystem::PAL_M:
        field1_lines = static_cast<size_t>(kPalMField1Lines);
        field1_on_even_rows = true;
        break;
      default:
        break;
    }
  }

  result.width = width;
  result.height = height;
  result.rgb_data.resize(static_cast<size_t>(width) * height * 3);

  for (uint32_t display_row = 0; display_row < height; ++display_row) {
    size_t buf_line;
    if (do_interlace) {
      const bool use_field1 = (display_row % 2 == 0) == field1_on_even_rows;
      buf_line =
          use_field1 ? (display_row / 2) : (field1_lines + display_row / 2);
      if (buf_line >= static_cast<size_t>(height)) {
        buf_line = static_cast<size_t>(height) - 1;
      }
    } else {
      buf_line = display_row;
    }

    const VideoFrameRepresentation::sample_type* line =
        representation->get_line(frame_id, buf_line);
    if (!line) continue;

    for (uint32_t x = 0; x < width; ++x) {
      uint8_t gray =
          scale_10bit_to_8bit(line[x], apply_level_scaling, black_level,
                              white_level, sync_tip_level, peak_level);
      size_t offset = (static_cast<size_t>(display_row) * width + x) * 3;
      result.rgb_data[offset + 0] = gray;
      result.rgb_data[offset + 1] = gray;
      result.rgb_data[offset + 2] = gray;
    }
  }

  // Dropout regions are not populated for frame-domain previews;
  // DropoutRun (frame-flat) and DropoutRegion (view-type) are distinct types.

  return result;
}

}  // namespace PreviewHelpers
}  // namespace orc
