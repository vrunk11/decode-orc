/*
 * File:        monodecoder.cpp
 * Module:      orc-core
 * Purpose:     Monochrome decoder
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2018-2026 Simon Inns
 * SPDX-FileCopyrightText: 2019-2021 Adam Sampson
 */

#include "monodecoder.h"

#include <cvbs_signal_constants.h>

#include <cstddef>

#include "../video_parameter_safety.h"
#include "comb.h"
#include "deemp.h"
#include "firfilter.h"
#include "palcolour.h"

MonoDecoder::MonoDecoder() {}

MonoDecoder::MonoDecoder(const MonoDecoder::MonoConfiguration& config) {
  monoConfig = config;
  const auto safety = ::orc::chroma_sink::sanitize_video_parameters(
      monoConfig.videoParameters,
      ::orc::chroma_sink::DecoderVideoProfile::Mono);

  if (!safety.warnings.empty()) {
    ORC_LOG_WARN(
        "MonoDecoder::MonoDecoder(): Adjusted unsafe video parameters: {}",
        ::orc::chroma_sink::join_issues(safety.warnings));
  }

  if (!safety.ok) {
    ORC_LOG_ERROR("MonoDecoder::MonoDecoder(): Invalid video parameters: {}",
                  ::orc::chroma_sink::join_issues(safety.errors));
    configurationValid_ = false;
    return;
  }

  monoConfig.videoParameters = safety.params;
  configurationValid_ = true;

  // Initialize comb filter if filterChroma is enabled
  if (monoConfig.filterChroma) {
    Comb::Configuration combConfig;
    combConfig.dimensions = 2;  // Use 2D comb filter
    combConfig.yNRLevel = 0.0;  // YNR handled separately in doYNR
    combConfig.cNRLevel = 0.0;  // No chroma NR needed for mono
    combConfig.chromaGain = 1.0;
    combConfig.chromaPhase = 0.0;
    combConfig.phaseCompensation = false;
    combConfig.showMap = false;
    combFilter = std::make_unique<Comb>();
    combFilter->updateConfiguration(monoConfig.videoParameters, combConfig);
  }
}

bool MonoDecoder::updateConfiguration(
    const ::orc::SourceParameters& videoParameters,
    const MonoDecoder::MonoConfiguration& configuration) {
  // This decoder works for both PAL and NTSC.
  monoConfig.yNRLevel = configuration.yNRLevel;
  monoConfig.filterChroma = configuration.filterChroma;
  const auto safety = ::orc::chroma_sink::sanitize_video_parameters(
      videoParameters, ::orc::chroma_sink::DecoderVideoProfile::Mono);

  if (!safety.warnings.empty()) {
    ORC_LOG_WARN(
        "MonoDecoder::updateConfiguration(): Adjusted unsafe video parameters: "
        "{}",
        ::orc::chroma_sink::join_issues(safety.warnings));
  }

  if (!safety.ok) {
    ORC_LOG_ERROR(
        "MonoDecoder::updateConfiguration(): Invalid video parameters: {}",
        ::orc::chroma_sink::join_issues(safety.errors));
    configurationValid_ = false;
    combFilter.reset();
    return false;
  }

  monoConfig.videoParameters = safety.params;
  configurationValid_ = true;

  // Create comb filter if needed
  if (monoConfig.filterChroma && !combFilter) {
    Comb::Configuration combConfig;
    combConfig.dimensions = 2;  // Use 2D comb filter
    combConfig.yNRLevel = 0.0;  // YNR handled separately in doYNR
    combConfig.cNRLevel = 0.0;  // No chroma NR needed for mono
    combConfig.chromaGain = 1.0;
    combConfig.chromaPhase = 0.0;
    combConfig.phaseCompensation = false;
    combConfig.showMap = false;
    combFilter = std::make_unique<Comb>();
    combFilter->updateConfiguration(videoParameters, combConfig);
  }

  return true;
}

bool MonoDecoder::configure(const ::orc::SourceParameters& videoParameters) {
  // This decoder works for both PAL and NTSC.
  const auto safety = ::orc::chroma_sink::sanitize_video_parameters(
      videoParameters, ::orc::chroma_sink::DecoderVideoProfile::Mono);

  if (!safety.warnings.empty()) {
    ORC_LOG_WARN(
        "MonoDecoder::configure(): Adjusted unsafe video parameters: {}",
        ::orc::chroma_sink::join_issues(safety.warnings));
  }

  if (!safety.ok) {
    ORC_LOG_ERROR("MonoDecoder::configure(): Invalid video parameters: {}",
                  ::orc::chroma_sink::join_issues(safety.errors));
    configurationValid_ = false;
    return false;
  }

  monoConfig.videoParameters = safety.params;
  configurationValid_ = true;

  return true;
}

void MonoDecoder::decodeFrames(const std::vector<SourceField>& inputFields,
                               int32_t startIndex, int32_t endIndex,
                               std::vector<ComponentFrame>& componentFrames) {
  if (!configurationValid_) {
    ORC_LOG_ERROR(
        "MonoDecoder::decodeFrames(): Decoder configuration is invalid");
    return;
  }

  const ::orc::SourceParameters& videoParameters = monoConfig.videoParameters;

  if (monoConfig.filterChroma && combFilter) {
    // Use comb filter to separate and remove chroma, like ld-chroma-decoder -b
    // This provides clean luma by filtering out the color subcarrier
    combFilter->decodeFrames(inputFields, startIndex, endIndex,
                             componentFrames);

    // The comb decoder outputs Y, U, V - we only want Y for monochrome
    // Zero out the U and V channels
    const int32_t lineOffset = videoParameters.active_area_cropping_applied
                                   ? videoParameters.first_active_frame_line
                                   : 0;
    const int32_t xOffset = videoParameters.active_area_cropping_applied
                                ? videoParameters.active_video_start
                                : 0;

    for (size_t frameIndex = 0; frameIndex < componentFrames.size();
         frameIndex++) {
      for (int32_t y = videoParameters.first_active_frame_line;
           y < videoParameters.last_active_frame_line; y++) {
        double* outU = componentFrames[frameIndex].u(y - lineOffset);
        double* outV = componentFrames[frameIndex].v(y - lineOffset);
        for (int32_t x = videoParameters.active_video_start;
             x < videoParameters.active_video_end; x++) {
          outU[x - xOffset] = 0.0;
          outV[x - xOffset] = 0.0;
        }
      }
    }
  } else {
    // Simple mode: just copy signal to Y
    // For composite sources: includes chroma subcarrier
    // For YC sources: use clean luma channel
    bool ignoreUV = false;

    const int32_t lineOffset = videoParameters.active_area_cropping_applied
                                   ? videoParameters.first_active_frame_line
                                   : 0;
    const int32_t xOffset = videoParameters.active_area_cropping_applied
                                ? videoParameters.active_video_start
                                : 0;

    // Check if this is a YC source
    bool is_yc_source = !inputFields.empty() && inputFields[0].is_yc;

    for (int32_t fieldIndex = startIndex, frameIndex = 0; fieldIndex < endIndex;
         fieldIndex += 2, frameIndex++) {
      componentFrames[frameIndex].init(videoParameters, ignoreUV);

      const SourceField* firstField =
          fieldIndex < static_cast<int32_t>(inputFields.size())
              ? &inputFields[fieldIndex]
              : nullptr;
      const SourceField* secondField =
          (fieldIndex + 1) < static_cast<int32_t>(inputFields.size())
              ? &inputFields[fieldIndex + 1]
              : nullptr;

      for (int32_t y = videoParameters.first_active_frame_line;
           y < videoParameters.last_active_frame_line; y++) {
        // Copy the signal to Y (leaving U and V blank)
        double* outY = componentFrames[frameIndex].y(y - lineOffset);
        const SourceField* sourceField = nullptr;
        if (firstField && ((y & 1) == firstField->getOffset())) {
          sourceField = firstField;
        } else if (secondField && ((y & 1) == secondField->getOffset())) {
          sourceField = secondField;
        }

        if (!sourceField) continue;

        const int32_t fieldLine = (y - sourceField->getOffset()) / 2;
        if (fieldLine < 0 ||
            static_cast<size_t>(fieldLine) >= sourceField->line_count) {
          continue;
        }

        // Use non-owning SourceField accessors — no data copy.
        const int16_t* inputLine =
            is_yc_source
                ? sourceField->getLumaLine(static_cast<size_t>(fieldLine))
                : sourceField->getLine(static_cast<size_t>(fieldLine));

        for (int32_t x = videoParameters.active_video_start;
             x < videoParameters.active_video_end; x++) {
          outY[x - xOffset] = inputLine[x];
        }
      }
      doYNR(componentFrames[frameIndex]);
    }
  }
}

void MonoDecoder::doYNR(ComponentFrame& componentFrame) {
  if (monoConfig.yNRLevel == 0.0) return;

  // 1. Compute coring level using CVBS spec constants (10-bit domain).
  // Use SourceParameters.white_level / blanking_level when available; fall
  // back to system-specific spec constants otherwise.
  double irescale;
  if (monoConfig.videoParameters.white_level > 0 &&
      monoConfig.videoParameters.blanking_level >= 0) {
    irescale = static_cast<double>(monoConfig.videoParameters.white_level -
                                   monoConfig.videoParameters.blanking_level) /
               100.0;
  } else {
    const bool isPal =
        monoConfig.videoParameters.system == orc::VideoSystem::PAL ||
        monoConfig.videoParameters.system == orc::VideoSystem::PAL_M;
    // EBU Tech. 3280-E (PAL) / SMPTE 244M-2003 (NTSC): 10-bit level ranges.
    irescale =
        isPal
            ? static_cast<double>(orc::kPalWhite - orc::kPalBlanking) / 100.0
            : static_cast<double>(orc::kNtscWhite - orc::kNtscBlanking) / 100.0;
  }
  double nr_y = monoConfig.yNRLevel * irescale;

  // 2. Choose filter taps & descriptor based on system
  bool usePal = (monoConfig.videoParameters.system == orc::VideoSystem::PAL ||
                 monoConfig.videoParameters.system == orc::VideoSystem::PAL_M);
  const auto& taps = usePal ? c_nrpal_b : c_nr_b;
  const auto& descriptor = usePal ? f_nrpal : f_nr;

  const int delay = static_cast<int>(taps.size()) / 2;

  const int32_t lineOffset =
      monoConfig.videoParameters.active_area_cropping_applied
          ? monoConfig.videoParameters.first_active_frame_line
          : 0;
  const int32_t xOffset =
      monoConfig.videoParameters.active_area_cropping_applied
          ? 0
          : monoConfig.videoParameters.active_video_start;

  // 3. Process each active scanline in the frame
  for (int line = monoConfig.videoParameters.first_active_frame_line;
       line < monoConfig.videoParameters.last_active_frame_line; ++line) {
    double* Y = componentFrame.y(line - lineOffset);

    // 4. High‑pass buffer & FIR filter
    std::vector<double> hpY(monoConfig.videoParameters.active_video_end +
                            delay);
    auto yFilter(descriptor);  // uses the chosen taps internally

    // Flush zeros before active start
    for (int x = monoConfig.videoParameters.active_video_start - delay;
         x < monoConfig.videoParameters.active_video_start; ++x) {
      yFilter.feed(0.0);
    }
    // Filter active region
    for (int x = monoConfig.videoParameters.active_video_start;
         x < monoConfig.videoParameters.active_video_end; ++x) {
      hpY[x] = yFilter.feed(Y[x - xOffset]);
    }
    // Flush zeros after active end
    for (int x = monoConfig.videoParameters.active_video_end;
         x < monoConfig.videoParameters.active_video_end + delay; ++x) {
      yFilter.feed(0.0);
    }

    // 5. Clamp & subtract
    for (int x = monoConfig.videoParameters.active_video_start;
         x < monoConfig.videoParameters.active_video_end; ++x) {
      double a = hpY[x + delay];
      if (std::fabs(a) > nr_y) a = (a > 0.0) ? nr_y : -nr_y;
      Y[x - xOffset] -= a;
    }
  }
}
