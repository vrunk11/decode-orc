/*
 * File:        preview_helpers.h
 * Module:      orc-core
 * Purpose:     Helper functions for implementing PreviewableStage
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef PREVIEW_HELPERS_H
#define PREVIEW_HELPERS_H

#include <memory>
#include <vector>

#include "preview_renderer.h"
#include "previewable_stage.h"
#include "video_frame_representation.h"

namespace orc {

/**
 * @brief Helper functions for stages implementing PreviewableStage interface
 *
 * These utilities provide standard preview rendering for
 * VideoFrameRepresentation data, eliminating code duplication across stages.
 */
namespace PreviewHelpers {

/**
 * @brief Generate standard preview options for a VideoFrameRepresentation
 *
 * Offers "interlaced_clamped", "interlaced_raw", "sequential_clamped",
 * "sequential_raw" options indexed by frame number.
 *
 * @param representation The video frame representation to preview
 * @return Vector of preview options (empty if representation is invalid)
 */
std::vector<PreviewOption> get_standard_preview_options(
    const std::shared_ptr<const VideoFrameRepresentation>& representation);

/**
 * @brief Render a standard preview from a VideoFrameRepresentation
 *
 * Supports option_ids "interlaced_clamped", "interlaced_raw",
 * "sequential_clamped", "sequential_raw".
 *
 * @param representation The video frame representation
 * @param option_id Preview option identifier
 * @param index Frame index (0-based)
 * @param hint Navigation hint (unused)
 * @return Preview image (invalid if option unknown or rendering fails)
 */
PreviewImage render_standard_preview(
    const std::shared_ptr<const VideoFrameRepresentation>& representation,
    const std::string& option_id, uint64_t index,
    PreviewNavigationHint hint = PreviewNavigationHint::Random);

}  // namespace PreviewHelpers

}  // namespace orc

#endif  // PREVIEW_HELPERS_H
