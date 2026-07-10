/*
 * File:        efm_audio_decode_stage_deps.h
 * Module:      orc-stage-plugin-efm_audio_decode
 * Purpose:     Default EFMAudioDecodeStage dependency implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <filesystem>

#include "efm_audio_decode_stage_deps_interface.h"

namespace orc {

// Production implementation: gathers the EFM t-value stream, runs the shared
// efm-decode pipeline (headerless PCM output) into a scratch cache file in
// the system temp directory, and serves stereo-pair reads by seeking into
// that file. Decoded audio is ~635 MB/hour, hence disk rather than RAM. The
// cache file is removed when this object is destroyed.
class EFMAudioDecodeDeps : public IEFMAudioDecodeDeps {
 public:
  EFMAudioDecodeDeps() = default;
  ~EFMAudioDecodeDeps() override;

  EFMAudioDecodeDeps(const EFMAudioDecodeDeps&) = delete;
  EFMAudioDecodeDeps& operator=(const EFMAudioDecodeDeps&) = delete;

  EFMAudioDecodeResult decode_to_cache(
      const VideoFrameRepresentation& representation,
      const EFMAudioDecodeOptions& options) override;

  std::vector<int16_t> read_cache_pairs(uint64_t first_pair,
                                        uint32_t pair_count) const override;

 private:
  std::filesystem::path cache_path_;  // empty until decode succeeds
};

}  // namespace orc
