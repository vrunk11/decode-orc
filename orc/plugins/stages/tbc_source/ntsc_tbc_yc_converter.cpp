/*
 * File:        ntsc_tbc_yc_converter.cpp
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     NTSC TBC YC variant — Y/C phase alignment check
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_tbc_yc_converter.h"

#include <string>

namespace orc {

bool NtscTBCYCConverter::check_yc_phase_alignment(
    int luma_colour_frame_index, int chroma_colour_frame_index) {
  // Both channels must agree on the colour frame position at frame 0.
  // When both are -1 (unmeasurable) the files are treated as aligned since
  // there is no information indicating a mismatch.
  return luma_colour_frame_index == chroma_colour_frame_index;
}

std::string NtscTBCYCConverter::yc_alignment_error(
    int luma_colour_frame_index, int chroma_colour_frame_index) {
  if (check_yc_phase_alignment(luma_colour_frame_index,
                               chroma_colour_frame_index)) {
    return {};
  }
  return "NTSC TBC YC phase mismatch at frame 0: luma colour_frame_index=" +
         std::to_string(luma_colour_frame_index) +
         ", chroma colour_frame_index=" +
         std::to_string(chroma_colour_frame_index) +
         ". The Y and C files are from different captures or are misaligned.";
}

}  // namespace orc
