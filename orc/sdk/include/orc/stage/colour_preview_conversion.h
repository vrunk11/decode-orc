/*
 * File:        colour_preview_conversion.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Render-boundary conversion from colour carriers to PreviewImage.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <orc/stage/orc_preview_carriers.h>
#include <orc/stage/orc_rendering.h>

namespace orc {

/**
 * @brief Convert a colour-domain carrier to display-target RGB888 preview.
 *
 * Conversion target is BT.709 primaries with sRGB transfer characteristics.
 * The source colorimetry is taken from carrier.colorimetry.
 */
PreviewImage render_preview_from_colour_carrier(
    const ColourFrameCarrier& carrier);

}  // namespace orc
