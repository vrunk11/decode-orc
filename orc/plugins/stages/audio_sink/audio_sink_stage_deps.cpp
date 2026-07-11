/*
 * File:        audio_sink_stage_deps.cpp
 * Module:      orc-core
 * Purpose:     AudioSinkStage dependency implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_sink_stage_deps.h"

#include <orc/stage/audio_channel_pair.h>
#include <orc/stage/logging.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

namespace orc {
namespace {

uint32_t clamp_to_uint32(uint64_t value) {
  return value > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
             ? std::numeric_limits<uint32_t>::max()
             : static_cast<uint32_t>(value);
}

// Narrow the 24-bit-in-int32 pipeline carrier to the writer's 16-bit sample
// width by dropping the low 8 bits (exact inverse of the << 8 ingest
// widening for 16-bit source material).
std::vector<int16_t> narrow_to_int16(const std::vector<int32_t>& samples) {
  std::vector<int16_t> narrowed;
  narrowed.reserve(samples.size());
  for (const int32_t sample : samples) {
    narrowed.push_back(static_cast<int16_t>(sample >> 8));
  }
  return narrowed;
}

}  // namespace

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
    const std::string& output_path, size_t pair) {
  const auto descriptor =
      representation->get_audio_channel_pair_descriptor(pair);
  if (!descriptor) {
    return {false, 0,
            "Audio channel pair " + std::to_string(pair) +
                " does not exist in the input (" +
                std::to_string(representation->audio_channel_pair_count()) +
                " channel pair(s) available)"};
  }

  const auto params = representation->get_video_parameters();
  const VideoSystem system = params ? params->system : VideoSystem::Unknown;

  // Per-frame pair counts are a model invariant of the video system
  // (SMPTE 272M-1994 §14.3 cadence), so the total payload — which the
  // stream-only writer must declare in the header up front — is known before
  // any samples are read.
  const auto frame_rng = representation->frame_range();
  const uint64_t total_pairs = audio_pair_offset(frame_rng.last + 1, system) -
                               audio_pair_offset(frame_rng.first, system);
  if (total_pairs == 0) {
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
    const uint16_t num_channels = 2;
    const uint16_t bits_per_sample = 16;

    // The RIFF header is 44 bytes (an even count), so it can be streamed
    // through the int16 sample writer without padding; WAV headers and
    // 16-bit PCM payloads are both little-endian byte sequences.
    const std::vector<uint8_t> header =
        build_wav_header(clamp_to_uint32(total_pairs), kAudioSampleRateHz,
                         num_channels, bits_per_sample);
    std::vector<int16_t> header_words(header.size() / 2);
    std::memcpy(header_words.data(), header.data(), header.size());
    writer->write(header_words);
  }

  const uint64_t total_frames = frame_rng.count();
  uint64_t pairs_written = 0;
  uint64_t current_frame = 0;
  for (FrameID fid = frame_rng.first; fid <= frame_rng.last; ++fid) {
    if (cancel_requested_ && cancel_requested_->load()) {
      writer->close();
      if (is_processing_) {
        is_processing_->store(false);
      }
      return {false, 0, "Cancelled by user"};
    }

    const auto samples = representation->get_audio_samples(pair, fid);
    if (samples.empty()) {
      // No audio for this frame: cadence-sized silence keeps the payload
      // aligned with the header and preserves A/V sync.
      const uint32_t silence_pairs = audio_pairs_in_frame(fid, system);
      writer->write(
          std::vector<int16_t>(static_cast<size_t>(silence_pairs) * 2, 0));
      pairs_written += silence_pairs;
    } else {
      const std::vector<int16_t> narrowed = narrow_to_int16(samples);
      writer->write(narrowed);
      pairs_written += narrowed.size() / 2;
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
  ORC_LOG_DEBUG("AudioSinkDeps: Wrote {} stereo pairs at {} Hz", pairs_written,
                kAudioSampleRateHz);
  return {true, pairs_written, ""};
}
}  // namespace orc
