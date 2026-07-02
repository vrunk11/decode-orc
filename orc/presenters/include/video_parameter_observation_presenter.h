/*
 * File:        video_parameter_observation_presenter.h
 * Module:      orc-presenters
 * Purpose:     Presenter for video parameter observer dialog
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <field_id.h>

#include <optional>

#include "video_parameter_observation_view_models.h"

namespace orc {
class ObservationContext;
class SourceParameters;
}  // namespace orc

namespace orc::presenters {

/**
 * @brief Extracts all video parameter observer values from an
 * ObservationContext for display in VideoParameterObserverDialog.
 */
class VideoParameterObservationPresenter {
 public:
  /**
   * @brief Extract all observed values for a single field.
   *
   * @param field_id         Field to extract observations for.
   * @param obs_context_ptr  Opaque pointer to ObservationContext.
   * @param video_params     Optional signal parameters (from
   * get_video_parameters()).
   * @return Populated view model; optional members absent when data
   * unavailable.
   */
  static VideoParameterObservationView extractObservations(
      FieldID field_id, const void* obs_context_ptr,
      const std::optional<orc::SourceParameters>& video_params);
};

}  // namespace orc::presenters
