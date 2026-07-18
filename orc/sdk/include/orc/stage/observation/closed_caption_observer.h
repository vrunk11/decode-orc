/*
 * File:        closed_caption_observer.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Closed caption observer (EIA-608 line 21/22)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

// SDK TIER: stage/observation — stage contract type crossing the plugin
// boundary. A layout change here bumps the host ABI version.

#include <orc/stage/observation/observer.h>

namespace orc {

class [[deprecated(
    "Obtain observations via "
    "IObservationService::create_observer(\"closed_caption\") "
    "(<orc/stage/observation/observation_service_interface.h>); this class "
    "leaves the plugin SDK next release.")]] ClosedCaptionObserver
    : public Observer {
 public:
  ClosedCaptionObserver() = default;
  ~ClosedCaptionObserver() override = default;

  std::string observer_name() const override { return "ClosedCaptionObserver"; }
  std::string observer_version() const override { return "1.0.0"; }

  void process_frame(const VideoFrameRepresentation& representation,
                     FrameID frame_id, IObservationContext& context) override;

  std::vector<ObservationKey> get_provided_observations() const override;

 private:
  struct DecodedCaption {
    uint8_t data0 = 0;
    uint8_t data1 = 0;
    bool parity_valid0 = false;
    bool parity_valid1 = false;
  };

  bool decode_line(const int16_t* line_data, size_t sample_count,
                   int16_t zero_crossing, size_t colorburst_end,
                   double samples_per_bit, DecodedCaption& decoded) const;
};

}  // namespace orc
