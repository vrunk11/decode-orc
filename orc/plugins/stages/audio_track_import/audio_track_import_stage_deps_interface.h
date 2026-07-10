/*
 * File:        audio_track_import_stage_deps_interface.h
 * Module:      orc-stage-plugin-audio_track_import
 * Purpose:     Dependency interface for AudioTrackImportStage (mockable WAV
 *              file probing and sample access seam)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace orc {

// Result of probing an external WAV file for import. The file is valid only
// when it is RIFF/WAVE, PCM, stereo, 16-bit — the sole encoding permitted by
// the CVBS file format specification for audio tracks.
struct WavProbeResult {
  bool valid{false};
  std::string error;        // human-readable reason when !valid
  uint32_t sample_rate{0};  // header sample rate in Hz
  uint64_t pair_count{0};   // stereo pairs in the data chunk
};

// Dependency seam for AudioTrackImportStage: probes the WAV header and serves
// stereo-pair reads from its data chunk. Split out so unit tests can mock the
// file access (no filesystem in unit tests).
//
// Thread safety: open() is called once before the wrapped representation is
// published; read_pairs() may then be called concurrently from any thread.
class IAudioTrackImportDeps {
 public:
  virtual ~IAudioTrackImportDeps() = default;

  // Opens and validates |wav_path|. On success the instance is bound to the
  // file and read_pairs() serves its data chunk.
  virtual WavProbeResult open(const std::string& wav_path) = 0;

  // Reads up to |pair_count| interleaved stereo pairs starting at
  // |first_pair| (pair offsets into the data chunk). Short reads at
  // end-of-file return fewer pairs; whole pairs only.
  virtual std::vector<int16_t> read_pairs(uint64_t first_pair,
                                          uint32_t pair_count) const = 0;
};

}  // namespace orc
