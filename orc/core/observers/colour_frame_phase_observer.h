/*
 * File:        colour_frame_phase_observer.h
 * Module:      orc-core
 * Purpose:     Colour-frame sequence index observer
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <vector>

#include "observer.h"

namespace orc {

// Measures the colour-frame sequence index for each frame and records it in
// the observation context.
//
// Demodulates the colour burst at the spec-defined reference position (line 9,
// burst window) and maps the measured carrier angle to the index.
// Results are stored under namespace "colour_frame_phase", key
// "colour_frame_index" (int32_t), keyed by FieldID(frame_id * 2).
//
// A value of -1 is stored when the burst is absent or too weak to classify
// (e.g. blank tape or pre-programme leader).
//
// EBU Tech. 3280-E §1.1.1 (PAL); SMPTE 244M-2003 §3.2 (NTSC);
// ITU-R BT.1700-1 Annex 1 Part B (PAL_M).
class ColourFramePhaseObserver : public Observer {
 public:
  ColourFramePhaseObserver() = default;
  ~ColourFramePhaseObserver() override = default;

  std::string observer_name() const override {
    return "ColourFramePhaseObserver";
  }

  std::string observer_version() const override { return "1.0.0"; }

  void process_frame(const VideoFrameRepresentation& representation,
                     FrameID frame_id, IObservationContext& context) override;

  std::vector<ObservationKey> get_provided_observations() const override {
    return {
        {"colour_frame_phase", "colour_frame_index", ObservationType::INT32,
         "Colour-frame sequence index (-1 when absent or unknown)",
         /*optional=*/true},
    };
  }
};

}  // namespace orc
