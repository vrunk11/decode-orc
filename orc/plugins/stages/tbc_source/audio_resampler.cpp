/*
 * File:        audio_resampler.cpp
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     SoXR-based audio resampler for NTSC/PAL_M frame-locked audio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "audio_resampler.h"

#include <soxr.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "logging.h"

namespace orc {

// ---------------------------------------------------------------------------
// resample
// ---------------------------------------------------------------------------

std::vector<int16_t> NtscPalMAudioResampler::resample(
    const std::vector<int16_t>& input_stereo, double in_rate, double out_rate) {
  if (input_stereo.empty()) return {};

  constexpr unsigned kChannels = 2;
  const size_t in_frames = input_stereo.size() / kChannels;
  if (in_frames == 0) return {};

  // Estimate output frame count with a small safety margin.
  const size_t out_estimate =
      static_cast<size_t>(
          std::lround(static_cast<double>(in_frames) * out_rate / in_rate)) +
      static_cast<size_t>(kChannels) * 16;

  std::vector<int16_t> output((out_estimate + 64) * kChannels, 0);

  // SoXR HQ quality, int16_t interleaved I/O.
  // SOXR_INT16_I = signed 16-bit interleaved (all channels in one array).
  const soxr_io_spec_t io_spec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
  const soxr_quality_spec_t quality = soxr_quality_spec(SOXR_HQ, 0);

  soxr_error_t err = nullptr;
  soxr_t soxr = soxr_create(in_rate, out_rate, kChannels, &err, &io_spec,
                            &quality, nullptr);
  if (!soxr || err) {
    ORC_LOG_ERROR("NtscPalMAudioResampler: soxr_create failed: {}",
                  err ? err : "null handle");
    if (soxr) soxr_delete(soxr);
    return {};
  }

  size_t idone = 0;
  size_t odone = 0;

  // Process input.
  err = soxr_process(soxr, input_stereo.data(), in_frames, &idone,
                     output.data(), out_estimate, &odone);
  if (err) {
    ORC_LOG_WARN("NtscPalMAudioResampler: soxr_process error: {}", err);
    soxr_delete(soxr);
    output.resize(odone * kChannels);
    return output;
  }

  // Flush residual samples (nullptr input signals end-of-stream to SoXR).
  const size_t headroom = (output.size() / kChannels) - odone;
  size_t odone2 = 0;
  soxr_process(soxr, nullptr, 0, nullptr, output.data() + odone * kChannels,
               headroom, &odone2);
  odone += odone2;

  soxr_delete(soxr);
  output.resize(odone * kChannels);
  return output;
}

// ---------------------------------------------------------------------------
// resample_and_segment
// ---------------------------------------------------------------------------

std::vector<std::vector<int16_t>> NtscPalMAudioResampler::resample_and_segment(
    const std::vector<int16_t>& raw_stereo_44100, size_t frame_count) {
  // NTSC/PAL_M locked rate: 44100000/1001 Hz ≈ 44055.944 Hz.
  constexpr double kInRate = 44100.0;
  constexpr double kOutRate = 44100000.0 / 1001.0;

  std::vector<std::vector<int16_t>> frames(frame_count);

  if (raw_stereo_44100.empty() || frame_count == 0) {
    for (auto& f : frames) {
      f.assign(kPairsPerFrame * 2, 0);
    }
    return frames;
  }

  const std::vector<int16_t> resampled =
      resample(raw_stereo_44100, kInRate, kOutRate);

  for (size_t i = 0; i < frame_count; ++i) {
    const size_t src_start = i * kPairsPerFrame * 2;
    const size_t src_end = src_start + kPairsPerFrame * 2;

    frames[i].resize(kPairsPerFrame * 2, 0);

    if (src_start < resampled.size()) {
      const size_t available = std::min(src_end, resampled.size()) - src_start;
      std::memcpy(frames[i].data(), resampled.data() + src_start,
                  available * sizeof(int16_t));
      // Remaining bytes are already zero from resize().
    }
    // When src_start >= resampled.size() the frame is silent (all zeros).
  }

  return frames;
}

}  // namespace orc
