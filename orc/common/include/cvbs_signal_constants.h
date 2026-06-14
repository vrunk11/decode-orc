/*
 * File:        cvbs_signal_constants.h
 * Module:      orc-common
 * Purpose:     Normative CVBS_U10_4FSC signal constants for PAL, NTSC, and
 * PAL_M
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <common_types.h>

#include <cstdint>
#include <utility>

// This header is the single authoritative source for CVBS_U10_4FSC signal
// constants.  No file other than this header may define these numeric values.
// Stages and tests must include this header and use the named constants.

namespace orc {

// ---------------------------------------------------------------------------
// PAL — EBU Tech. 3280-E
// ---------------------------------------------------------------------------

// EBU Tech. 3280-E §1.1: PAL colour subcarrier frequency.
constexpr double kPalFsc = 4'433'618.75;

// EBU Tech. 3280-E §1.1: 4FSC sample rate = 4 × fsc.
constexpr double kPalSampleRate = 17'734'475.0;

// EBU Tech. 3280-E §1.1: Nominal samples per line (non-integer due to
// non-orthogonal sampling).
constexpr double kPalSamplesPerLine = 1135.0064;

// EBU Tech. 3280-E §1.1: Total samples in a complete PAL frame.
// = 625 lines × 1135 samples + 4 extra samples on non-orthogonal lines.
constexpr int32_t kPalFrameSamples = 709'379;

// EBU Tech. 3280-E §1.1: Total lines in a PAL frame.
constexpr int32_t kPalFrameLines = 625;

// EBU Tech. 3280-E §1.3: Lines in PAL field 1 (the 313-line field; CVBS
// convention places the longer field first in the flat frame buffer).
constexpr int32_t kPalField1Lines = 313;

// EBU Tech. 3280-E §1.3: Maximum samples on any PAL line (non-orthogonal
// lines carry one extra sample).
constexpr int32_t kPalMaxSamplesPerLine = 1136;

// EBU Tech. 3280-E: Normative CVBS_U10_4FSC signal levels (10-bit domain).
constexpr int32_t kPalSyncTip = 4;
constexpr int32_t kPalBlanking = 256;
constexpr int32_t kPalBlack = 282;
constexpr int32_t kPalWhite = 844;
constexpr int32_t kPalPeak = 1019;

// EBU Tech. 3280-E §1.3.1: 0-based frame-flat line indices of the four lines
// that carry 1136 samples instead of 1135.  All four values are 0-based and
// within [0, kPalFrameLines-1] = [0, 624].
//
// Field 1 (313 lines, frame-flat indices 0–312):
//   non-orthogonal at 0-based positions 155 and 311.
// Field 2 (312 lines, frame-flat indices 313–624):
//   non-orthogonal at 0-based frame-flat positions 313+155=468 and 313+311=624.
//
// These are derived from the continued-fraction expansion of 709,379/625 and
// verified by: 621×1135 + 4×1136 = 704,835 + 4,544 = 709,379 =
// kPalFrameSamples.
constexpr int32_t kPalExtraSampleLines[4] = {155, 311, 313 + 155, 313 + 311};

// ---------------------------------------------------------------------------
// NTSC — SMPTE 244M-2003
// ---------------------------------------------------------------------------

// SMPTE 244M-2003 §4.1: NTSC colour subcarrier frequency.
constexpr double kNtscFsc = 315.0e6 / 88.0;

// SMPTE 244M-2003 §4.1: 4FSC sample rate = 4 × fsc.
constexpr double kNtscSampleRate = 4.0 * kNtscFsc;

// SMPTE 244M-2003 §4.1: Samples per line (orthogonal, exact integer).
constexpr int32_t kNtscSamplesPerLine = 910;

// SMPTE 244M-2003 §4.1: Total samples in a complete NTSC frame.
// = 525 lines × 910 samples/line.
constexpr int32_t kNtscFrameSamples = 477'750;

// SMPTE 244M-2003 §3.1: Total lines in an NTSC frame.
constexpr int32_t kNtscFrameLines = 525;

// SMPTE 244M-2003 §3.2: Lines in NTSC VFR field 1 (top spatial field).
// The top field is sourced from ld-decode TBC field 2 (263 real lines).
// ld-decode TBC field 1 (earlier temporal, 262 real lines) becomes VFR field 2.
constexpr int32_t kNtscField1Lines = 263;

// SMPTE 244M-2003: Normative CVBS_U10_4FSC signal levels (10-bit domain).
constexpr int32_t kNtscSyncTip = 16;
constexpr int32_t kNtscBlanking = 240;
constexpr int32_t kNtscBlack = 252;
constexpr int32_t kNtscWhite = 800;
constexpr int32_t kNtscPeak = 988;

// ---------------------------------------------------------------------------
// PAL_M — ITU-R BT.1700-1 Annex 1 Part B
// ---------------------------------------------------------------------------

// ITU-R BT.1700-1 Annex 1 Part B: PAL_M colour subcarrier frequency.
// Derived from: fsc = (909/4) × (525 × 30000/1001) Hz.
constexpr double kPalMFsc = (909.0 / 4.0) * (525.0 * 30000.0 / 1001.0);

// ITU-R BT.1700-1 Annex 1 Part B: 4FSC sample rate = 4 × fsc.
constexpr double kPalMSampleRate = 4.0 * kPalMFsc;

// ITU-R BT.1700-1 Annex 1 Part B: Samples per line (orthogonal, exact integer).
constexpr int32_t kPalMSamplesPerLine = 909;

// ITU-R BT.1700-1 Annex 1 Part B: Total samples in a complete PAL_M frame.
// = 525 lines × 909 samples/line.
constexpr int32_t kPalMFrameSamples = 477'225;

// ITU-R BT.1700-1 Annex 1 Part B: Total lines in a PAL_M frame.
constexpr int32_t kPalMFrameLines = 525;

// ITU-R BT.1700-1 Annex 1 Part B: Lines in PAL_M VFR field 1 (top spatial
// field). Same swap convention as NTSC: TBC field 2 (263 lines) → VFR field 1
// (top).
constexpr int32_t kPalMField1Lines = 263;

// PAL_M signal levels are identical to NTSC (same line count and blanking).
// Use kNtscSyncTip, kNtscBlanking, kNtscBlack, kNtscWhite, kNtscPeak.

// ---------------------------------------------------------------------------
// ld-decode TBC 16-bit domain normative levels
// ---------------------------------------------------------------------------
// The ld-decode .tbc format stores samples as uint16_t with the following
// standard mapping.  kTbcBlanking / kTbcWhite are the round-trip reference
// values used when converting CVBS_U10_4FSC ↔ TBC 16-bit.
constexpr int32_t kTbcBlanking = 16384;  // 0 IRE blanking (0x4000 = 25 %)
constexpr int32_t kTbcWhite = 54400;     // 100 IRE white  (≈ 83 % of 65535)

// ---------------------------------------------------------------------------
// Colour burst sample range constants
// ---------------------------------------------------------------------------
// Sample offsets within a TBC line that locate the colour burst window.
// These are nominal values fixed by the video standard; per-disc values may
// deviate by a few samples for damaged or non-standard tapes, but spec-defined
// ranges are sufficient for dropout-correction masking and observer windowing.

// EBU Tech. 3280-E Table 1: PAL colour burst sample range.
constexpr int32_t kPalColourBurstStart = 98;
constexpr int32_t kPalColourBurstEnd = 138;

// SMPTE 244M-2003 Table 1: NTSC colour burst sample range.
// PAL_M uses the same values as NTSC.
constexpr int32_t kNtscColourBurstStart = 72;
constexpr int32_t kNtscColourBurstEnd = 108;

// Return the (start, end) colour burst sample range for the given video system.
inline std::pair<int32_t, int32_t> colour_burst_range(VideoSystem sys) {
  if (sys == VideoSystem::PAL) {
    return {kPalColourBurstStart, kPalColourBurstEnd};
  }
  // NTSC and PAL_M share the same burst window.
  return {kNtscColourBurstStart, kNtscColourBurstEnd};
}

// ---------------------------------------------------------------------------
// System-derived sample rate and subcarrier frequency helpers
// ---------------------------------------------------------------------------
// Both values are fully determined by the video system; reading them from
// per-disc metadata and re-storing in SourceParameters adds round-trip noise.

// Return the 4FSC sample rate (Hz) for the given video system.
// EBU Tech. 3280-E §1.1 (PAL) / SMPTE 244M-2003 §4.1 (NTSC) /
// ITU-R BT.1700-1 Annex 1 Part B (PAL_M).
inline double sample_rate_from_system(VideoSystem sys) {
  switch (sys) {
    case VideoSystem::PAL:
      return kPalSampleRate;
    case VideoSystem::PAL_M:
      return kPalMSampleRate;
    default:
      return kNtscSampleRate;
  }
}

// Return the colour subcarrier frequency (Hz) for the given video system.
// EBU Tech. 3280-E §1.1 (PAL) / SMPTE 244M-2003 §4.1 (NTSC) /
// ITU-R BT.1700-1 Annex 1 Part B (PAL_M).
inline double fsc_from_system(VideoSystem sys) {
  switch (sys) {
    case VideoSystem::PAL:
      return kPalFsc;
    case VideoSystem::PAL_M:
      return kPalMFsc;
    default:
      return kNtscFsc;
  }
}

}  // namespace orc
