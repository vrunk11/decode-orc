/*
 * File:        vectorscope_extract.h
 * Module:      chroma-decoder
 * Purpose:     Extract vectorscope U/V samples from decoded ComponentFrames
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef CHROMA_DECODER_VECTORSCOPE_EXTRACT_H
#define CHROMA_DECODER_VECTORSCOPE_EXTRACT_H

#include <orc/stage/orc_source_parameters.h>
#include <orc/stage/preview/orc_vectorscope.h>

#include <cstdint>

// Forward declaration (ComponentFrame is in global namespace, not orc::)
class ComponentFrame;

namespace orc {

/**
 * @brief Extract vectorscope data directly from ComponentFrame U/V channels.
 *
 * Uses the native U/V chroma values written by the comb decoder into
 * ComponentFrame, normalised to ±32767 by the blanking-to-white range.
 */
VectorscopeData extract_vectorscope_from_component_frame(
    const ::ComponentFrame& frame, const SourceParameters& video_parameters,
    uint64_t field_number, uint32_t subsample = 1);

}  // namespace orc

#endif  // CHROMA_DECODER_VECTORSCOPE_EXTRACT_H
