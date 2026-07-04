/*
 * File:        audio_sink_stage_deps.cpp
 * Module:      orc-core
 * Purpose:     AudioSinkStage dependency implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_sink_stage_deps.h"

#include <orc/stage/logging.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

namespace orc {
void AudioSinkStageDeps::init(TriggerProgressCallback progress_callback,
                              std::atomic<bool>* is_processing,
                              std::atomic<bool>* cancel_requested) {
  progress_callback_ = std::move(progress_callback);
  is_processing_ = is_processing;
  cancel_requested_ = cancel_requested;
}

std::vector<uint8_t> AudioSinkStageDeps::build_wav_header(
    uint32_t num_samples, uint32_t sample_rate, uint16_t num_channels,
    uint16_t bits_per_sample) const {
  uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
  uint16_t block_align = num_channels * (bits_per_sample / 8);
  uint32_t data_size = num_samples * num_channels * (bits_per_sample / 8);
  uint32_t file_size = 36 + data_size;

  std::vector<uint8_t> header;
  header.reserve(44);
  auto append = [&header](const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    header.insert(header.end(), bytes, bytes + size);
  };

  append("RIFF", 4);
  append(&file_size, 4);
  append("WAVE", 4);

  append("fmt ", 4);
  uint32_t fmt_size = 16;
  append(&fmt_size, 4);

  uint16_t audio_format = 1;
  append(&audio_format, 2);
  append(&num_channels, 2);
  append(&sample_rate, 4);
  append(&byte_rate, 4);
  append(&block_align, 2);
  append(&bits_per_sample, 2);

  append("data", 4);
  append(&data_size, 4);

  return header;
}

AudioSinkWriteResult AudioSinkStageDeps::write_audio_wav(
    const VideoFrameRepresentation* representation,
    const std::string& output_path) {
  // Free-running audio is not accessible per-frame; only locked audio is
  // available via get_audio_samples(FrameID).
  if (!representation->audio_locked()) {
    return {false, 0, "Audio is not locked to frames; cannot export"};
  }

  auto frame_rng = representation->frame_range();
  const FrameID start_frame = frame_rng.first;
  const FrameID end_frame = frame_rng.last;
  const uint64_t total_frames = frame_rng.count();

  uint64_t total_samples = 0;
  for (FrameID fid = start_frame; fid <= end_frame; ++fid) {
    total_samples += representation->get_audio_sample_count(fid);
  }

  if (total_samples == 0) {
    return {false, 0, "No audio samples found in frame range"};
  }

  std::shared_ptr<IFileWriterInt16> writer;
  if (stage_services_) {
    writer = stage_services_->create_buffered_file_writer_int16(
        static_cast<size_t>(4 * 1024 * 1024));
  }
  if (!writer) {
    return {false, 0, "File writer service unavailable"};
  }
  if (!writer->open(output_path)) {
    return {false, 0, "Failed to open output file: " + output_path};
  }

  {
    const uint32_t sample_rate = 44100;
    const uint16_t num_channels = 2;
    const uint16_t bits_per_sample = 16;
    const uint32_t wav_samples =
        total_samples >
                static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(total_samples);

    // The RIFF header is 44 bytes (an even count), so it can be streamed
    // through the int16 sample writer without padding; WAV headers and
    // 16-bit PCM payloads are both little-endian byte sequences.
    const std::vector<uint8_t> header = build_wav_header(
        wav_samples, sample_rate, num_channels, bits_per_sample);
    std::vector<int16_t> header_words(header.size() / 2);
    std::memcpy(header_words.data(), header.data(), header.size());
    writer->write(header_words);
  }

  uint64_t frames_written = 0;
  uint64_t current_frame = 0;
  for (FrameID fid = start_frame; fid <= end_frame; ++fid) {
    if (cancel_requested_ && cancel_requested_->load()) {
      writer->close();
      if (is_processing_) {
        is_processing_->store(false);
      }
      return {false, 0, "Cancelled by user"};
    }

    auto samples = representation->get_audio_samples(fid);
    if (!samples.empty()) {
      writer->write(samples);
      frames_written += samples.size() / 2;
    }

    ++current_frame;
    if (progress_callback_ && current_frame % 10 == 0) {
      progress_callback_(current_frame, total_frames,
                         "Writing audio frame " +
                             std::to_string(current_frame) + "/" +
                             std::to_string(total_frames));
    }
  }

  writer->close();
  ORC_LOG_DEBUG("AudioSinkDeps: Wrote {} audio frames", frames_written);
  return {true, frames_written, ""};
}
}  // namespace orc
