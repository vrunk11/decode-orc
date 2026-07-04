/*
 * File:        audio_resampler.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     SoXR-based audio resampler for NTSC/PAL_M frame-locked audio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orc {

// ---------------------------------------------------------------------------
// NtscPalMAudioResampler
// ---------------------------------------------------------------------------
// Converts raw stereo PCM recorded at 44100 Hz to the NTSC/PAL_M frame-locked
// rate of 44100000/1001 Hz ≈ 44055.944 Hz, then segments the result into
// fixed-size per-frame blocks.
//
// Background:
//   ld-decode records audio at a nominal 44100 Hz, but NTSC/PAL_M video runs
//   at 30000/1001 fps.  At the frame-locked rate the audio block per frame is
//   exactly 44100000/1001 / (30000/1001) = 1470 stereo pairs.
//
//   PAL audio (25 fps, 44100/25 = 1764 pairs/frame) is already at the locked
//   rate and does NOT pass through this class.
//
// Usage:
//   std::vector<int16_t> raw = read_whole_pcm(path);
//   auto frames = NtscPalMAudioResampler::resample_and_segment(raw,
//   frame_count);
//   // frames[i] contains exactly kNtscAudioPairsPerFrame × 2 int16_t values.
//
// Thread-safety: stateless static methods — safe to call from any thread.
class NtscPalMAudioResampler {
 public:
  // NTSC/PAL_M: fixed stereo pairs per frame after resampling.
  // = 44100000/1001 / (30000/1001) = 44100000/30000 = 1470 exactly.
  static constexpr size_t kPairsPerFrame = 1470;

  // Resample interleaved stereo int16_t PCM from in_rate to out_rate Hz using
  // SoXR HQ resampling.  Returns interleaved stereo int16_t at out_rate.
  //
  // Returns empty vector on error (e.g. SoXR allocation failure).
  static std::vector<int16_t> resample(const std::vector<int16_t>& input_stereo,
                                       double in_rate, double out_rate);

  // Resample raw PCM and split into frame_count blocks of kPairsPerFrame
  // stereo pairs each.  Short final block is zero-padded.
  //
  // in_rate  = 44100.0 (raw PCM)
  // out_rate = 44100000.0 / 1001.0 (frame-locked NTSC/PAL_M)
  static std::vector<std::vector<int16_t>> resample_and_segment(
      const std::vector<int16_t>& raw_stereo_44100, size_t frame_count);
};

}  // namespace orc
