/*
 * File:        ntsc_tbc_yc_converter.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     NTSC TBC YC variant — Y/C phase alignment check
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <string>

namespace orc {

// ---------------------------------------------------------------------------
// NtscTBCYCConverter
// ---------------------------------------------------------------------------
// Phase alignment helpers for NTSC TBC sources that carry separate luma (Y)
// and chroma (C) TBC files.
//
// Phase alignment check (design §14.11):
//   At open time, compare colour_frame_index at frame 0 for the Y and C
//   channels.  A mismatch indicates the files are mis-paired and must be
//   hard-rejected with a clear error.  Both channels must yield the same
//   colour_frame_index; if not, the source is unusable.
//
// SMPTE 244M-2003 §3.2: NTSC colour_frame_index is 0 (frame A) or 1
// (frame B).  A mismatch means the Y and C files are from different points
// in the 2-frame sequence.
class NtscTBCYCConverter {
 public:
  // Check whether luma and chroma channels are aligned at frame 0.
  //
  // Returns true when both colour_frame_index values agree (or when both are
  // -1, meaning unmeasurable).  Returns false when they differ; callers must
  // reject the source with a clear error identifying the phase mismatch.
  static bool check_yc_phase_alignment(int luma_colour_frame_index,
                                       int chroma_colour_frame_index);

  // Returns an error string when the Y/C phase is misaligned, or empty string
  // when aligned.
  static std::string yc_alignment_error(int luma_colour_frame_index,
                                        int chroma_colour_frame_index);
};

}  // namespace orc
