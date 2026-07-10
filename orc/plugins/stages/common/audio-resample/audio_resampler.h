/*
 * File:        audio_resampler.h
 * Module:      orc-audio-resample (shared stage-plugin library)
 * Purpose:     SoXR-based stereo audio resampling between the free-running
 *              44100 Hz rate and the frame-locked rates
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <orc/stage/audio_track.h>
#include <orc/stage/common_types.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orc {

// ---------------------------------------------------------------------------
// AudioResampler
// ---------------------------------------------------------------------------
// Shared free-running ↔ frame-locked stereo resampler used by the tbc_source,
// cvbs_source, and audio_sink stage plugins.
//
// Rates (CVBS file format spec — Audio Data):
//   Free-running          — 44100 Hz.
//   Frame-locked PAL      — 44100 Hz (already the free-running rate);
//                           25 fps × 1764 stereo pairs per frame.
//   Frame-locked NTSC/PAL-M — 44100000/1001 Hz ≈ 44055.944 Hz;
//                           30000/1001 fps × 1470 stereo pairs per frame.
//
// Resampling uses SoXR HQ quality and is duration- and sync-preserving.
//
// Thread-safety: stateless static methods — safe to call from any thread.
class AudioResampler {
 public:
  // The NTSC/PAL-M frame-locked rate as a double, for SoXR.
  static constexpr double kNtscLockedRateHz = 44100000.0 / 1001.0;

  // The free-running rate as a double, for SoXR.
  static constexpr double kFreeRunningRateHz = 44100.0;

  // Resample interleaved stereo int16_t PCM from in_rate to out_rate Hz using
  // SoXR HQ resampling.  Returns interleaved stereo int16_t at out_rate.
  //
  // Returns empty vector on error (e.g. SoXR allocation failure).
  static std::vector<int16_t> resample(const std::vector<int16_t>& input_stereo,
                                       double in_rate, double out_rate);

  // Convert a free-running 44100 Hz stereo stream into frame_count
  // frame-locked blocks for the given video system:
  //   PAL          — same-rate segmentation into 1764-pair blocks (no
  //                  resampling; 44100 Hz is already the locked rate).
  //   NTSC / PAL-M — SoXR HQ resample to 44100000/1001 Hz, then segmentation
  //                  into 1470-pair blocks.
  // Short final blocks are zero-padded; blocks past the end of the stream are
  // silent.  Always returns frame_count blocks; an unknown video system has
  // no locked layout and yields empty blocks.
  static std::vector<std::vector<int16_t>> lock_and_segment(
      const std::vector<int16_t>& raw_stereo_44100, VideoSystem system,
      size_t frame_count);
};

}  // namespace orc
