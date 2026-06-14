/*
 * File:        ntsc_tbc_converter.cpp
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     NTSC TBC level mapping and frame assembly into CVBS_U10_4FSC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_tbc_converter.h"

#include <cmath>
#include <stdexcept>

namespace orc {

// ---------------------------------------------------------------------------
// Level mapping
// ---------------------------------------------------------------------------

int16_t NtscTBCConverter::tbc_to_cvbs(uint16_t tbc_sample, int32_t tbc_blanking,
                                      int32_t tbc_white) {
  // SMPTE 244M-2003: linear mapping from TBC 16-bit domain to CVBS_U10_4FSC.
  // No output clamping — headroom outside [kNtscSyncTip, kNtscPeak] is
  // preserved.
  const double n = static_cast<double>(static_cast<int32_t>(tbc_sample) -
                                       tbc_blanking) /
                   static_cast<double>(tbc_white - tbc_blanking);
  const double cvbs =
      n * static_cast<double>(kNtscWhite - kNtscBlanking) + kNtscBlanking;
  return static_cast<int16_t>(std::lround(cvbs));
}

// ---------------------------------------------------------------------------
// Frame assembly
// ---------------------------------------------------------------------------

std::vector<int16_t> NtscTBCConverter::assemble_frame(
    const std::vector<uint16_t>& tbc_field1,  // 262 × 910 samples
    const std::vector<uint16_t>& tbc_field2,  // 263 × 910 samples
    int32_t tbc_blanking, int32_t tbc_white) {
  // SMPTE 244M-2003 §4.1 / SMPTE 170M-2004 §11.3: NTSC frame assembly.
  // TBC field ordering (ld-decode convention):
  //   TBC field 1 = odd/earlier temporal, 262 real lines → VFR field 2 (bottom)
  //   TBC field 2 = even/later temporal,  263 real lines → VFR field 1 (top)
  // VFR layout: [CVBS field 1 (top, 263 lines)][CVBS field 2 (bottom, 262 lines)]
  // Matches PAL convention: TBC field 2 → VFR field 1 (top spatial field).
  // NTSC is orthogonal: all lines have kNtscSamplesPerLine = 910 samples.
  constexpr int32_t kTBCF1Lines = kNtscFrameLines - kNtscField1Lines;  // 262
  constexpr int32_t kTBCF2Lines = kNtscField1Lines;                    // 263
  constexpr int32_t kLineW = kNtscSamplesPerLine;                      // 910

  const size_t exp_f1 =
      static_cast<size_t>(kTBCF1Lines) * static_cast<size_t>(kLineW);
  const size_t exp_f2 =
      static_cast<size_t>(kTBCF2Lines) * static_cast<size_t>(kLineW);

  if (tbc_field1.size() != exp_f1 || tbc_field2.size() != exp_f2) {
    throw std::invalid_argument(
        "NtscTBCConverter::assemble_frame: unexpected field sample counts");
  }

  std::vector<int16_t> frame;
  frame.reserve(static_cast<size_t>(kNtscFrameSamples));

  // VFR field 1 (top, 263 lines) ← TBC field 2 (later temporal)
  for (const uint16_t s : tbc_field2) {
    frame.push_back(tbc_to_cvbs(s, tbc_blanking, tbc_white));
  }
  // VFR field 2 (bottom, 262 lines) ← TBC field 1 (earlier temporal)
  for (const uint16_t s : tbc_field1) {
    frame.push_back(tbc_to_cvbs(s, tbc_blanking, tbc_white));
  }

  return frame;
}

// ---------------------------------------------------------------------------
// Colour frame sequence
// ---------------------------------------------------------------------------

int NtscTBCConverter::map_field_phase_to_colour_frame_index(
    std::optional<int32_t> field_phase_id) {
  // SMPTE 244M-2003 §3.2: NTSC 2-frame A/B colour sequence.
  // ld-decode encodes the NTSC phase as field_phase_id 1 (frame A) or 2
  // (frame B).  Two consecutive fields of one frame share the same id.
  if (!field_phase_id.has_value()) return -1;
  switch (field_phase_id.value()) {
    case 1:
      return 0;  // frame A
    case 2:
      return 1;  // frame B
    default:
      return -1;
  }
}

}  // namespace orc
