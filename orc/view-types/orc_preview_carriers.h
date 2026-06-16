/*
 * File:        orc_preview_carriers.h
 * Module:      orc-view-types
 * Purpose:     Typed preview carriers used by the Phase 2 preview pipeline.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "orc_preview_types.h"
#include "orc_vectorscope.h"

namespace orc {

/**
 * @brief Colour-domain preview carrier with high-precision component planes.
 *
 * This carrier is intentionally display-agnostic. It preserves decoded Y/U/V
 * component values in a high-precision processing domain, along with explicit
 * colorimetric metadata. Conversion to display-target RGB is performed only at
 * the rendering boundary.
 */
struct ColourFrameCarrier {
  VideoDataType data_type{VideoDataType::ColourNTSC};
  ColorimetricMetadata colorimetry{};
  VideoSystem system{VideoSystem::Unknown};

  uint64_t frame_index{0};
  uint32_t width{0};
  uint32_t height{0};
  uint32_t active_x_start{0};
  uint32_t active_x_end{0};
  uint32_t active_y_start{0};
  uint32_t active_y_end{0};

  // Decoder-domain component planes. Each plane is width * height samples.
  std::vector<double> y_plane;
  std::vector<double> u_plane;
  std::vector<double> v_plane;

  std::optional<VectorscopeData> vectorscope_data;

  // CVBS_U10_4FSC anchor points used to normalize Y/U/V before matrix
  // conversion. Set by chroma_sink from SourceParameters. Values are in the
  // 10-bit CVBS domain (e.g. PAL: 256/256/844, NTSC: 240/282/800).
  // cvbs_black is the picture-black floor (= blanking for PAL, +7.5 IRE for
  // NTSC/PAL_M). Y is normalized relative to cvbs_black; U/V use the full
  // active range (cvbs_white - cvbs_blanking) so chroma scale is unaffected.
  double cvbs_blanking{0.0};
  double cvbs_black{0.0};
  double cvbs_white{1023.0};

  bool is_valid() const {
    if (width == 0 || height == 0) {
      return false;
    }

    const size_t expected =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    if (y_plane.size() != expected || u_plane.size() != expected ||
        v_plane.size() != expected) {
      return false;
    }

    return cvbs_white > cvbs_blanking;
  }
};

}  // namespace orc
