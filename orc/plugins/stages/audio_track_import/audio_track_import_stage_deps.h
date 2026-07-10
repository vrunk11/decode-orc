/*
 * File:        audio_track_import_stage_deps.h
 * Module:      orc-stage-plugin-audio_track_import
 * Purpose:     Production dependencies for AudioTrackImportStage (WAV file
 *              probing and sample reads)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "audio_track_import_stage_deps_interface.h"

namespace orc {

// Production implementation: walks the RIFF chunk list to validate the
// format ("fmt " chunk: PCM, stereo, 16-bit) and locate the "data" chunk,
// then serves seek-and-read pair access into it.
class AudioTrackImportDeps : public IAudioTrackImportDeps {
 public:
  WavProbeResult open(const std::string& wav_path) override;
  std::vector<int16_t> read_pairs(uint64_t first_pair,
                                  uint32_t pair_count) const override;

 private:
  std::string wav_path_;
  uint64_t data_offset_ = 0;  // byte offset of the data chunk payload
  uint64_t data_pairs_ = 0;   // stereo pairs in the data chunk
};

}  // namespace orc
