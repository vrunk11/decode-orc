/*
 * File:        audio_resampler.cpp
 * Module:      orc-audio-resample (shared stage-plugin library)
 * Purpose:     SoXR-based stereo audio resampling between the free-running
 *              44100 Hz rate and the frame-locked rates
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "audio_resampler.h"

#include <orc/stage/logging.h>
#include <soxr.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orc {

// ---------------------------------------------------------------------------
// resample
// ---------------------------------------------------------------------------

std::vector<int16_t> AudioResampler::resample(
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
    ORC_LOG_ERROR("AudioResampler: soxr_create failed: {}",
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
    ORC_LOG_WARN("AudioResampler: soxr_process error: {}", err);
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
// lock_and_segment
// ---------------------------------------------------------------------------

std::vector<std::vector<int16_t>> AudioResampler::lock_and_segment(
    const std::vector<int16_t>& raw_stereo_44100, VideoSystem system,
    size_t frame_count) {
  const size_t pairs_per_frame =
      static_cast<size_t>(locked_audio_pairs_per_frame(system));

  std::vector<std::vector<int16_t>> frames(frame_count);
  if (pairs_per_frame == 0) return frames;

  if (raw_stereo_44100.empty() || frame_count == 0) {
    for (auto& f : frames) {
      f.assign(pairs_per_frame * 2, 0);
    }
    return frames;
  }

  // PAL locked audio is already 44100 Hz — segmentation only. NTSC/PAL-M is
  // pulled down to 44100000/1001 Hz first (SoXR HQ, duration-preserving).
  std::vector<int16_t> resampled;
  const std::vector<int16_t>* locked_stream = &raw_stereo_44100;
  if (system != VideoSystem::PAL) {
    resampled =
        resample(raw_stereo_44100, kFreeRunningRateHz, kNtscLockedRateHz);
    locked_stream = &resampled;
  }

  for (size_t i = 0; i < frame_count; ++i) {
    const size_t src_start = i * pairs_per_frame * 2;
    const size_t src_end = src_start + pairs_per_frame * 2;

    frames[i].resize(pairs_per_frame * 2, 0);

    if (src_start < locked_stream->size()) {
      const size_t available =
          std::min(src_end, locked_stream->size()) - src_start;
      std::memcpy(frames[i].data(), locked_stream->data() + src_start,
                  available * sizeof(int16_t));
      // Remaining values are already zero from resize().
    }
    // When src_start >= locked_stream->size() the frame is silent (all zeros).
  }

  return frames;
}

}  // namespace orc
