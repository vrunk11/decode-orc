/*
 * File:        pal_m_tbc_yc_converter.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     PAL_M TBC YC variant — Y/C phase alignment check
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <string>

#include "tbc_yc_alignment.h"

namespace orc {

// ---------------------------------------------------------------------------
// PalMTBCYCConverter
// ---------------------------------------------------------------------------
// Phase alignment helpers for PAL_M TBC sources that carry separate luma (Y)
// and chroma (C) TBC files.
//
// Phase alignment check (design §14.11):
//   At open time, compare colour_frame_index at frame 0 for the Y and C
//   channels.  A mismatch indicates the files are mis-paired and must be
//   hard-rejected with a clear error.  Both channels must yield the same
//   colour_frame_index on every frame; if not, the source is unusable.
//
// ITU-R BT.1700-1 Annex 1 Part B: PAL_M colour_frame_index is 1–4 (same
// 4-frame cycle as PAL).  A mismatch means the Y and C files are from
// different points in the 4-frame sequence.
class PalMTBCYCConverter {
 public:
  // Check whether luma and chroma channels are aligned at frame 0.
  static bool check_yc_phase_alignment(int luma_colour_frame_index,
                                       int chroma_colour_frame_index) {
    return orc::check_yc_phase_alignment(luma_colour_frame_index,
                                         chroma_colour_frame_index);
  }

  // Returns an error string when the Y/C phase is misaligned, or empty when
  // aligned.
  static std::string yc_alignment_error(int luma_colour_frame_index,
                                        int chroma_colour_frame_index) {
    return orc::yc_alignment_error("PAL-M", luma_colour_frame_index,
                                   chroma_colour_frame_index);
  }
};

}  // namespace orc
