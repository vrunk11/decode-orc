/*
 * File:        dropout_presenter.h
 * Module:      orc-presenters
 * Purpose:     Dropout editing presenter - MVP architecture
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <orc/stage/frame_id.h>
#include <orc/stage/node_id.h>
#include <orc/stage/preview/orc_rendering.h>  // For DropoutRegion

#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward declare core Project type
namespace orc {
class Project;
}  // namespace orc

// Forward declare presenters
namespace orc::presenters {
class IProjectPresenter;
}

namespace orc::presenters {

// Use public API DropoutRegion
using DropoutRegion = orc::DropoutRegion;

/**
 * @brief Per-frame dropout map view model
 *
 * Coordinates are frame-flat 0-based (line = frame-flat line index,
 * start_sample/end_sample = sample within that line).
 */
struct FrameDropoutMap {
  FrameID frame_id;
  std::vector<DropoutRegion> additions;  ///< Dropouts to add
  std::vector<DropoutRegion> removals;   ///< Dropouts to remove

  FrameDropoutMap() : frame_id(0) {}
  explicit FrameDropoutMap(FrameID id) : frame_id(id) {}
};

/**
 * @brief DropoutPresenter - Reads and writes dropout_map stage parameters
 *
 * This presenter extracts dropout-map editing logic from the GUI layer,
 * translating a dropout_map stage's serialised parameter string to and from
 * per-frame FrameDropoutMap view models for the dropout editor.
 *
 * All coordinates are frame-flat 0-based (line = frame-flat line index,
 * start_sample/end_sample = sample within that line), matching the
 * DropoutMapStage serialisation format.
 */
class DropoutPresenter {
 public:
  /**
   * @brief Construct presenter for a project
   * @param project_presenter ProjectPresenter to delegate project operations to
   */
  explicit DropoutPresenter(
      orc::presenters::IProjectPresenter& project_presenter);

  /**
   * @brief Destructor
   */
  ~DropoutPresenter();

  // Disable copy, enable move
  DropoutPresenter(const DropoutPresenter&) = delete;
  DropoutPresenter& operator=(const DropoutPresenter&) = delete;
  DropoutPresenter(DropoutPresenter&&) noexcept;
  DropoutPresenter& operator=(DropoutPresenter&&) noexcept;

  /**
   * @brief Get dropout map for a node (if it's a dropout_map stage)
   * @param node_id Dropout map stage node
   * @return Map of frame IDs to dropout modifications
   */
  std::map<uint64_t, FrameDropoutMap> getDropoutMap(NodeID node_id);

  /**
   * @brief Set dropout map for a node (if it's a dropout_map stage)
   * @param node_id Dropout map stage node
   * @param dropout_map New dropout map (keyed by frame_id)
   * @return true on success
   */
  bool setDropoutMap(NodeID node_id,
                     const std::map<uint64_t, FrameDropoutMap>& dropout_map);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace orc::presenters
