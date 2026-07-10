/*
 * File:        efm_audio_decode_stage_deps_interface.h
 * Module:      orc-stage-plugin-efm_audio_decode
 * Purpose:     Dependency interface for EFMAudioDecodeStage (mockable decode
 *              and cache-file access seam)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <orc/stage/video_frame_representation.h>

#include <cstdint>
#include <string>
#include <vector>

namespace orc {

// Decode options forwarded to the shared EFM decode pipeline (a subset of
// efm_sink's audio-mode options).
struct EFMAudioDecodeOptions {
  bool no_timecodes{false};
  bool no_audio_concealment{false};
};

struct EFMAudioDecodeResult {
  bool success{false};
  std::string error_message;      // human-readable reason when !success
  uint64_t stream_pair_count{0};  // decoded 44.1 kHz stereo pairs available
};

// Dependency seam for EFMAudioDecodeStage: performs the whole-stream EFM
// decode into a scratch cache and serves stereo-pair reads from it. Split out
// so unit tests can mock the decode and file access (no filesystem in unit
// tests).
//
// Thread safety: decode_to_cache() is called at most once per instance (the
// caller serialises via std::call_once); read_cache_pairs() may be called
// from any thread after decode_to_cache() has returned successfully.
class IEFMAudioDecodeDeps {
 public:
  virtual ~IEFMAudioDecodeDeps() = default;

  // Accumulates the representation's EFM t-value stream across its full frame
  // range, decodes it to headerless 44.1 kHz interleaved stereo int16_t PCM
  // in a scratch cache file, and reports the decoded pair count.
  virtual EFMAudioDecodeResult decode_to_cache(
      const VideoFrameRepresentation& representation,
      const EFMAudioDecodeOptions& options) = 0;

  // Reads up to |pair_count| stereo pairs starting at |first_pair| from the
  // cache written by decode_to_cache(). Returns interleaved L,R samples;
  // short reads are truncated to whole pairs. Empty when no cache exists.
  virtual std::vector<int16_t> read_cache_pairs(uint64_t first_pair,
                                                uint32_t pair_count) const = 0;
};

}  // namespace orc
