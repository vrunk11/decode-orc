/*
 * File:        ntsc_tbc_converter.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     NTSC TBC level mapping and frame assembly into CVBS_U10_4FSC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cvbs_signal_constants.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace orc {

// ---------------------------------------------------------------------------
// NtscTBCConverter
// ---------------------------------------------------------------------------
// Stateless helper class for NTSC TBC → CVBS_U10_4FSC conversion.
//
// SMPTE 244M-2003 §4.1: NTSC 4FSC sampling at 910 samples/line.
// SMPTE 244M-2003 §3.1: 525 lines/frame, 2:1 interlace.
//
// TBC field ordering (SMPTE 244M-2003 §3.2):
//   ld-decode stores both fields at 263 lines in the TBC file.
//   TBC field 1 (even file index) = odd/earlier temporal field, 262 real lines.
//   TBC field 2 (odd file index)  = even/later temporal field, 263 real lines.
//   CVBS frame layout: [CVBS field 1 = TBC field 1 (262 lines)]
//                       [CVBS field 2 = TBC field 2 (263 lines)]
//   No field order swap is required for NTSC; the TBC field numbering matches
//   the CVBS convention directly.
class NtscTBCConverter {
 public:
  // -------------------------------------------------------------------------
  // Level mapping
  // -------------------------------------------------------------------------

  // SMPTE 244M-2003: map one TBC 16-bit unsigned sample to CVBS_U10_4FSC.
  //
  // tbc_blanking / tbc_white are the 16-bit TBC-domain level values from
  // `.tbc.json.db` (blanking_16b_ire, white_16b_ire).  No output clamping:
  // headroom below sync tip and above peak white is preserved in the int16_t
  // result.
  static int16_t tbc_to_cvbs(uint16_t tbc_sample, int32_t tbc_blanking,
                              int32_t tbc_white);

  // -------------------------------------------------------------------------
  // Frame assembly
  // -------------------------------------------------------------------------

  // Assemble a CVBS_U10_4FSC NTSC frame from two TBC fields.
  //
  // tbc_field1: kNtscField1Lines = 262 lines × kNtscSamplesPerLine = 910
  //   The TBC file stores both fields at 263 lines; the caller strips the
  //   final padding line and passes exactly 262 × 910 = 238,220 samples.
  //
  // tbc_field2: (kNtscFrameLines - kNtscField1Lines) = 263 lines × 910
  //   = 239,530 samples.
  //
  // SMPTE 244M-2003 §4.1: NTSC is orthogonal — all lines have exactly
  // kNtscSamplesPerLine = 910 samples; no extra samples are inserted.
  //
  // Output: [field 1: 262 × 910][field 2: 263 × 910] = kNtscFrameSamples.
  static std::vector<int16_t> assemble_frame(
      const std::vector<uint16_t>& tbc_field1,  // 262 × 910 samples
      const std::vector<uint16_t>& tbc_field2,  // 263 × 910 samples
      int32_t tbc_blanking, int32_t tbc_white);

  // -------------------------------------------------------------------------
  // Colour frame sequence
  // -------------------------------------------------------------------------

  // SMPTE 244M-2003 §3.2: map a TBC field_phase_id to FrameDescriptor
  // colour_frame_index (0 = frame A, 1 = frame B) for the NTSC 2-frame cycle.
  //
  // In ld-decode the NTSC field_phase_id is 1 (frame A) or 2 (frame B).
  // Two consecutive fields of the same frame share the same field_phase_id.
  //
  //   field_phase_id 1 → colour_frame_index 0  (frame A)
  //   field_phase_id 2 → colour_frame_index 1  (frame B)
  //   absent or out of range → -1
  static int map_field_phase_to_colour_frame_index(
      std::optional<int32_t> field_phase_id);
};

}  // namespace orc
