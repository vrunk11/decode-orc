/*
 * File:        tbc_yc_alignment.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     Shared Y/C phase alignment helpers for TBC source stages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <string>

namespace orc {

// Check whether luma and chroma colour_frame_index values agree at frame 0.
//
// Both channels must carry the same colour frame position for a Y/C source to
// be usable.  When both are -1 (unmeasurable) the files are treated as aligned
// since there is no information indicating a mismatch.
//
// Returns true when aligned, false when misaligned.
inline bool check_yc_phase_alignment(int luma_colour_frame_index,
                                     int chroma_colour_frame_index) {
  return luma_colour_frame_index == chroma_colour_frame_index;
}

// Returns a human-readable rejection message when the Y/C phase is misaligned,
// or an empty string when aligned.  format_name is the video system label
// inserted into the message (e.g. "PAL", "NTSC", "PAL-M").
inline std::string yc_alignment_error(const char* format_name,
                                      int luma_colour_frame_index,
                                      int chroma_colour_frame_index) {
  if (luma_colour_frame_index == chroma_colour_frame_index) return {};
  return std::string(format_name) +
         " TBC YC phase mismatch at frame 0: luma colour_frame_index=" +
         std::to_string(luma_colour_frame_index) +
         ", chroma colour_frame_index=" +
         std::to_string(chroma_colour_frame_index) +
         ". The Y and C files are from different captures or are misaligned.";
}

}  // namespace orc
