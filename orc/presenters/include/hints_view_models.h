/*
 * File:        hints_view_models.h
 * Module:      orc-presenters
 * Purpose:     View-facing data models for GUI/CLI layers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

namespace orc::presenters {

/**
 * @brief Video system/format enumeration for presenter layer
 */
enum class VideoSystem {
  PAL,    // 625-line PAL
  NTSC,   // 525-line NTSC
  PAL_M,  // 525-line PAL
  Unknown
};

/**
 * @brief Video parameters view model for presenter layer
 *
 * Contains all video format and timing parameters needed by GUI.
 * Mirrors core VideoParameters but in presenter layer.
 */
struct VideoParametersView {
  // Format
  VideoSystem system = VideoSystem::Unknown;

  // Frame geometry — canonical field names matching SourceParameters.
  // frame_width_nominal: nominal samples per line (1135 PAL, 910 NTSC, 909
  // PAL_M).
  int frame_width_nominal = -1;

  // Sample ranges
  int color_burst_start = -1;
  int color_burst_end = -1;
  int active_video_start = -1;
  int active_video_end = -1;

  // CVBS_U10_4FSC 10-bit domain signal levels (from SourceParameters).
  // -1 means not populated (source has not been migrated to Phase 3+ pipeline).
  int32_t sync_tip_level = -1;
  int32_t blanking_level = -1;
  int32_t black_level = -1;
  int32_t white_level = -1;
  int32_t peak_level = -1;

  // CVBS_U10_4FSC DC level of the chroma signal for YC sources (-1 = N/A).
  int32_t chroma_dc_offset = -1;
};

}  // namespace orc::presenters

// Include public API types that are used
#include <orc/stage/orc_source_parameters.h>

namespace orc::presenters {

/**
 * @brief Convert core SourceParameters to presenter VideoParametersView
 *
 * This helper function encapsulates the conversion logic to avoid
 * duplication across the codebase.
 */
VideoParametersView toVideoParametersView(const orc::SourceParameters& params);

// ============================================================================
// Field/Frame Coordinate Conversion Helpers
// ============================================================================
// These helpers convert between field coordinates (field index, field line)
// and frame coordinates (frame number, frame line) for display purposes.

/**
 * @brief Result of field-to-frame conversion for display
 */
struct FieldToFrameDisplayResult {
  uint64_t frame_number;  ///< 1-based frame number for display
  int frame_line_number;  ///< 1-based frame line number (1 to 525/625)
  bool is_first_field;    ///< True if this is the first field of the frame
};

/**
 * @brief Convert field coordinates to frame coordinates for display
 *
 * @param system Video system (NTSC, PAL, PAL_M)
 * @param field_index 0-based field index
 * @param field_line_number 1-based line number within the field
 * @return Frame number and line for display, or nullopt if invalid
 */
std::optional<FieldToFrameDisplayResult> fieldToFrameCoordinates(
    VideoSystem system, uint64_t field_index, int field_line_number);

}  // namespace orc::presenters
