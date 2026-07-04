/*
 * File:        orc_histogram.h
 * Module:      orc-view-types
 * Purpose:     Public API types for video histogram visualization
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ORC_PUBLIC_ORC_HISTOGRAM_H
#define ORC_PUBLIC_ORC_HISTOGRAM_H

#include <orc/stage/common_types.h>

#include <array>
#include <cstdint>

namespace orc {

/**
 * @brief Video histogram data for Y, U, V, I, and Q components.
 *
 * Histogram bins span a normalised signal range of kRangeMin% to kRangeMax%
 * where 0 % = blanking level (black) and 100 % = white level.  The extra
 * headroom below 0 % and above 100 % captures out-of-legal-range samples so
 * that the EBU R103 tolerance zones can be displayed.
 *
 * Y (luma) is normalised relative to the picture-black floor (cvbs_black).
 * U and V are normalised relative to the full active-video swing
 * (cvbs_white − cvbs_blanking), centred at zero.
 * I and Q are derived from U/V using the SMPTE 170M-2004 §7.3 33° rotation
 * and are only populated for NTSC.
 */
struct VideoHistogramData {
  // Number of bins per channel.
  static constexpr size_t kBinCount = 256;

  // Luma (Y) range: 0 % = black, 100 % = white.  Extra headroom exposes
  // out-of-legal-range content for EBU R103 zone display.
  static constexpr double kRangeMin = -10.0;
  static constexpr double kRangeMax = 110.0;

  // Chroma (U/V/I/Q) range: 0 % = neutral (no colour).  The signal swings
  // symmetrically negative and positive, so the range is centred at 0 %
  // with headroom beyond ±100 % to capture clipped chroma.
  static constexpr double kChromaRangeMin = -110.0;
  static constexpr double kChromaRangeMax = 110.0;

  std::array<uint32_t, kBinCount> y_bins{};  ///< Luma histogram
  std::array<uint32_t, kBinCount> u_bins{};  ///< Cb/U chroma histogram
  std::array<uint32_t, kBinCount> v_bins{};  ///< Cr/V chroma histogram
  std::array<uint32_t, kBinCount> i_bins{};  ///< I component (NTSC only)
  std::array<uint32_t, kBinCount> q_bins{};  ///< Q component (NTSC only)

  VideoSystem system{VideoSystem::Unknown};
  uint64_t field_number{0};
  uint32_t width{0};
  uint32_t height{0};

  // CVBS_U10_4FSC anchor points (same convention as VectorscopeData).
  // Y is normalised relative to cvbs_black; U/V use (cvbs_white −
  // cvbs_blanking) as the full-swing reference.
  double cvbs_blanking{0.0};
  double cvbs_black{0.0};
  double cvbs_white{1023.0};

  uint32_t total_pixels{0};  ///< Total pixels accumulated across all bins
};

}  // namespace orc

#endif  // ORC_PUBLIC_ORC_HISTOGRAM_H
