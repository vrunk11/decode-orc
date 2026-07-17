/*
 * File:        preview_stage_types.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Shared lightweight types for stage preview interfaces
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

// SDK TIER: stage/preview — stage contract type crossing the plugin boundary.
// A layout change here bumps the host ABI version.

#include <cstdint>
#include <string>
#include <vector>

namespace orc {

/**
 * @brief Hint about how preview frames are being requested.
 *
 * Allows the rendering infrastructure to inform a stage whether to optimise
 * for sequential playback or random access (scrubbing).
 */
enum class PreviewNavigationHint {
  Sequential,  ///< Next/Previous buttons — may pre-fetch adjacent frames
  Random       ///< Slider scrubbing — single frame only, no pre-fetch
};

/**
 * @brief Describes a single preview mode offered by a stage.
 *
 * Used by IStageCustomPreviewRenderer::get_preview_options() and by
 * PreviewHelpers::get_standard_preview_options().
 */
struct PreviewOption {
  std::string id;            ///< Unique key (e.g. "field", "frame")
  std::string display_name;  ///< Human-readable label shown in the GUI
  bool is_rgb;      ///< True if rendered as RGB, false if grayscale/YUV
  uint32_t width;   ///< Width of rendered images in samples
  uint32_t height;  ///< Height of rendered images in lines
  uint64_t count;   ///< Number of navigable items (fields or frames)
  double
      dar_aspect_correction;  ///< Horizontal scale factor for 4:3 DAR display
};

}  // namespace orc
