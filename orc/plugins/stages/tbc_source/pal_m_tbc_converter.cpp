/*
 * File:        pal_m_tbc_converter.cpp
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     PAL_M TBC level mapping and frame assembly into CVBS_U10_4FSC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_m_tbc_converter.h"

#include <cmath>
#include <stdexcept>

namespace orc {

// ---------------------------------------------------------------------------
// Level mapping
// ---------------------------------------------------------------------------

int16_t PalMTBCConverter::tbc_to_cvbs(uint16_t tbc_sample, int32_t tbc_blanking,
                                      int32_t tbc_white) {
  // ITU-R BT.1700-1 Annex 1 Part B: PAL_M uses NTSC signal levels.
  // Linear mapping: no output clamping — headroom is preserved.
  const double n = static_cast<double>(static_cast<int32_t>(tbc_sample) -
                                       tbc_blanking) /
                   static_cast<double>(tbc_white - tbc_blanking);
  // kNtscWhite and kNtscBlanking apply to PAL_M (identical numeric values).
  const double cvbs =
      n * static_cast<double>(kNtscWhite - kNtscBlanking) + kNtscBlanking;
  return static_cast<int16_t>(std::lround(cvbs));
}

// ---------------------------------------------------------------------------
// Frame assembly
// ---------------------------------------------------------------------------

std::vector<int16_t> PalMTBCConverter::assemble_frame(
    const std::vector<uint16_t>& tbc_field1,  // 262 × 909 samples
    const std::vector<uint16_t>& tbc_field2,  // 263 × 909 samples
    int32_t tbc_blanking, int32_t tbc_white) {
  // ITU-R BT.1700-1 Annex 1 Part B §1: PAL_M frame assembly.
  // Field 1: 262 lines.  Field 2: 263 lines.  Orthogonal: 909 samples/line.
  constexpr int32_t kF1Lines = kPalMField1Lines;                    // 262
  constexpr int32_t kF2Lines = kPalMFrameLines - kPalMField1Lines;  // 263
  constexpr int32_t kLineW = kPalMSamplesPerLine;                   // 909

  const size_t exp_f1 =
      static_cast<size_t>(kF1Lines) * static_cast<size_t>(kLineW);
  const size_t exp_f2 =
      static_cast<size_t>(kF2Lines) * static_cast<size_t>(kLineW);

  if (tbc_field1.size() != exp_f1 || tbc_field2.size() != exp_f2) {
    throw std::invalid_argument(
        "PalMTBCConverter::assemble_frame: unexpected field sample counts");
  }

  std::vector<int16_t> frame;
  frame.reserve(static_cast<size_t>(kPalMFrameSamples));

  for (const uint16_t s : tbc_field1) {
    frame.push_back(tbc_to_cvbs(s, tbc_blanking, tbc_white));
  }
  for (const uint16_t s : tbc_field2) {
    frame.push_back(tbc_to_cvbs(s, tbc_blanking, tbc_white));
  }

  return frame;
}

// ---------------------------------------------------------------------------
// Colour frame sequence
// ---------------------------------------------------------------------------

int PalMTBCConverter::map_field_phase_to_colour_frame_index(
    std::optional<int32_t> field_phase_id) {
  // ITU-R BT.1700-1 Annex 1 Part B: PAL_M 4-frame colour cycle.
  // ld-decode encodes the PAL_M phase identically to PAL: field_phase_id 1–8,
  // pairs mapping to colour frame positions 1–4.
  if (!field_phase_id.has_value()) return -1;
  const int32_t phase = field_phase_id.value();
  if (phase < 1 || phase > 8) return -1;
  return ((phase - 1) / 2) + 1;  // 1-based, cycles 1-4
}

}  // namespace orc
