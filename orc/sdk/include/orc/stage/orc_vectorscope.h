/*
 * File:        orc_vectorscope.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Public API for vectorscope visualization data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef ORC_PUBLIC_ORC_VECTORSCOPE_H
#define ORC_PUBLIC_ORC_VECTORSCOPE_H

#include <orc/stage/common_types.h>  // For VideoSystem enum

#include <cstdint>
#include <vector>

namespace orc {

/**
 * @brief Single U/V sample point for vectorscope display
 */
struct UVSample {
  double u;          ///< U (Cb) component: -32768 to +32767 range
  double v;          ///< V (Cr) component: -32768 to +32767 range
  uint8_t field_id;  ///< Field index (0 = first/odd, 1 = second/even)

  UVSample() : u(0), v(0), field_id(0) {}
  UVSample(double u_val, double v_val, uint8_t field = 0)
      : u(u_val), v(v_val), field_id(field) {}
};

/**
 * @brief Vectorscope data extracted from a decoded RGB field
 *
 * Contains all U/V chroma samples for vectorscope visualization along with
 * video parameters needed for graticule rendering and color accuracy targets.
 */
struct VectorscopeData {
  std::vector<UVSample> samples;  ///< All U/V samples from the field
  uint32_t width;                 ///< Field width in pixels
  uint32_t height;                ///< Field height in lines
  uint64_t field_number;          ///< Field number for identification

  // CVBS_U10_4FSC anchor points for graticule/targets (10-bit domain).
  VideoSystem system = VideoSystem::Unknown;  ///< Video system (NTSC/PAL)
  int32_t cvbs_white = 0;     ///< White level (100 IRE) in CVBS_U10_4FSC
  int32_t cvbs_blanking = 0;  ///< Blanking level (0 IRE) in CVBS_U10_4FSC

  VectorscopeData() : width(0), height(0), field_number(0) {}
};

}  // namespace orc

#endif  // ORC_PUBLIC_ORC_VECTORSCOPE_H
