/*
 * File:        video_metadata_types.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Video metadata types exposed through VFR interface
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <array>
#include <cstdint>

namespace orc {

/**
 * @brief VBI (Vertical Blanking Interval) data
 *
 * It contains VBI data that can be used for display or further processing.
 */
struct VbiData {
  bool in_use = false;
  std::array<int32_t, 3> vbi_data = {0, 0, 0};
};

}  // namespace orc
