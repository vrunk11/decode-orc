/*
 * File:        stage_custom_preview_renderer.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Interface for stages with non-standard preview rendering.
 *
 *              Implemented only by stages whose preview output cannot be
 *              derived from their primary VFR output via DAGFrameRenderer (e.g.
 *              multi-output stages such as SourceAlignStage).  All other stages
 *              should implement IStagePreviewCapability only; the renderer
 *              obtains their preview images directly from the DAG VFR.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

// SDK TIER: stage/preview — stage contract type crossing the plugin boundary.
// A layout change here bumps the host ABI version.

#include <orc/stage/preview/orc_rendering.h>
#include <orc/stage/preview/preview_stage_types.h>

#include <string>
#include <vector>

namespace orc {

/**
 * @brief Custom preview rendering interface for multi-output or
 *        non-standard-VFR stages.
 *
 * Stages that implement this interface alongside IStagePreviewCapability
 * are responsible for both declaring available preview options and rendering
 * them.  This path is the exception: the normal path is the renderer using
 * DAGFrameRenderer + PreviewHelpers directly on the stage's VFR output.
 */
class IStageCustomPreviewRenderer {
 public:
  virtual ~IStageCustomPreviewRenderer() = default;

  /**
   * @brief Return the list of preview options this stage can render.
   *
   * Called after execution; returns an empty vector if no data is loaded.
   */
  virtual std::vector<PreviewOption> get_preview_options() const = 0;

  /**
   * @brief Render one preview image for the given option and index.
   *
   * @param option_id  ID from get_preview_options() (e.g. "source_0")
   * @param index      0-based item index within the option
   * @param hint       Sequential or Random navigation hint
   * @return           Rendered RGB888 image; invalid if rendering failed
   */
  virtual PreviewImage render_preview(const std::string& option_id,
                                      uint64_t index,
                                      PreviewNavigationHint hint) const = 0;
};

}  // namespace orc
