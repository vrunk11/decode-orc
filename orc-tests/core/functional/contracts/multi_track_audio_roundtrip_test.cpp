/*
 * File:        multi_track_audio_roundtrip_test.cpp
 * Module:      orc-core-functional-tests
 * Purpose:     Channel-pair audio round-trip through cvbs_sink and
 *              cvbs_source (real files: WAV sidecars and the .meta
 *              audio_track table)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <gtest/gtest.h>
#include <orc/stage/artifact.h>
#include <orc/stage/audio_channel_pair.h>
#include <orc/stage/cvbs_signal_constants.h>
#include <orc/stage/observation_context.h>
#include <orc/stage/video_frame_representation.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cvbs_sink_stage_deps.h"
#include "cvbs_source_stage.h"

namespace orc_functional_test {

namespace {

constexpr size_t kFrameCount = 2;
// SMPTE 272M-1994: PAL carries 1920 stereo pairs per frame at 48 kHz.
constexpr uint32_t kPalPairsPerFrame = 1920;

// Deterministic 24-bit-in-int32 sample generators.  Values are multiples of
// 256 (zero low byte) so the container's 16-bit narrow (>> 8) and the
// source's widen (<< 8) round-trip exactly.
int32_t pair0_sample(orc::FrameID frame_id, size_t index) {
  const int64_t modulo = static_cast<int64_t>(
      (static_cast<uint64_t>(frame_id) * 1000 + index) % 30000);
  return static_cast<int32_t>(modulo - 15000) << 8;
}

int32_t pair1_sample(orc::FrameID frame_id, size_t index) {
  const int64_t modulo = static_cast<int64_t>(
      (static_cast<uint64_t>(frame_id) * 700 + index * 7) % 20000);
  return static_cast<int32_t>(modulo - 10000) << 8;
}

// Two PAL frames of synthetic video with two audio channel pairs:
// pair 0 "Analogue" (ANALOGUE), pair 1 "EFM digital audio" (EFM).
class ChannelPairVFR : public orc::VideoFrameRepresentation,
                       public orc::Artifact {
 public:
  ChannelPairVFR()
      : orc::Artifact(orc::ArtifactID("channel_pair_vfr"), orc::Provenance{}) {
    for (size_t f = 0; f < kFrameCount; ++f) {
      std::vector<sample_type> frame(orc::kPalFrameSamples);
      for (size_t i = 0; i < frame.size(); ++i) {
        frame[i] = static_cast<sample_type>((f * 17 + i) % 1024);
      }
      frames_.push_back(std::move(frame));
    }
  }

  std::string type_name() const override { return "ChannelPairVFR"; }

  orc::FrameIDRange frame_range() const override {
    return {0, kFrameCount - 1};
  }
  size_t frame_count() const override { return kFrameCount; }
  bool has_frame(orc::FrameID id) const override { return id < kFrameCount; }

  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (id >= kFrameCount) return std::nullopt;
    orc::FrameDescriptor desc;
    desc.frame_id = id;
    desc.system = orc::VideoSystem::PAL;
    desc.height = 625;
    desc.samples_total = orc::kPalFrameSamples;
    desc.samples_per_line_nominal = 1135;
    return desc;
  }

  const sample_type* get_frame(orc::FrameID id) const override {
    return (id < kFrameCount) ? frames_[id].data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(orc::FrameID id) const override {
    return (id < kFrameCount) ? frames_[id] : std::vector<sample_type>{};
  }

  // Audio channel pairs
  size_t audio_channel_pair_count() const override { return 2; }

  std::optional<orc::AudioChannelPairDescriptor>
  get_audio_channel_pair_descriptor(size_t pair) const override {
    if (pair == 0) {
      return orc::AudioChannelPairDescriptor{"Analogue",
                                             orc::AudioOrigin::ANALOGUE};
    }
    if (pair == 1) {
      return orc::AudioChannelPairDescriptor{"EFM digital audio",
                                             orc::AudioOrigin::EFM};
    }
    return std::nullopt;
  }

  std::vector<int32_t> get_audio_samples(size_t pair,
                                         orc::FrameID id) const override {
    if (pair > 1 || id >= kFrameCount) return {};
    std::vector<int32_t> samples(static_cast<size_t>(orc::audio_pairs_in_frame(
                                     id, orc::VideoSystem::PAL)) *
                                 2);
    for (size_t i = 0; i < samples.size(); ++i) {
      samples[i] = (pair == 0) ? pair0_sample(id, i) : pair1_sample(id, i);
    }
    return samples;
  }

 private:
  std::vector<std::vector<sample_type>> frames_;
};

// nSamplesPerSec from a WAV file's RIFF fmt header (LE word at byte 24).
uint32_t read_wav_header_rate(const std::string& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.is_open()) return 0;
  ifs.seekg(24, std::ios::beg);
  uint8_t bytes[4] = {0, 0, 0, 0};
  ifs.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (!ifs.good()) return 0;
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

}  // namespace

TEST(MultiTrackAudioRoundTrip, CVBSSinkToCVBSSource_PreservesChannelPairs) {
  const std::filesystem::path dir =
      std::filesystem::path(::testing::TempDir()) / "orc_multi_track_roundtrip";
  std::filesystem::create_directories(dir);
  const std::string base = (dir / "roundtrip").string();

  // --- Write with the real cvbs_sink deps ---
  ChannelPairVFR vfr;
  std::atomic<bool> cancel{false};
  orc::CVBSSinkStageDeps sink_deps;
  sink_deps.init({}, &cancel);

  orc::CVBSSinkWriteConfig config;
  config.output_base_path = base;
  const auto write_result = sink_deps.write_cvbs(&vfr, config);
  ASSERT_TRUE(write_result.success) << write_result.status_message;
  EXPECT_EQ(write_result.frames_written, kFrameCount);

  EXPECT_TRUE(std::filesystem::exists(base + ".composite"));
  EXPECT_TRUE(std::filesystem::exists(base + ".meta"));
  ASSERT_TRUE(std::filesystem::exists(base + "_audio_00.wav"));
  ASSERT_TRUE(std::filesystem::exists(base + "_audio_01.wav"));

  // Each pair file: 44-byte header + 2 frames × 1920 pairs × 4 bytes
  // (16-bit stereo container payload) — equal-length by construction.
  const uintmax_t expected_size = 44u + kFrameCount * kPalPairsPerFrame * 4u;
  EXPECT_EQ(std::filesystem::file_size(base + "_audio_00.wav"), expected_size);
  EXPECT_EQ(std::filesystem::file_size(base + "_audio_01.wav"), expected_size);

  // The pipeline is 48 kHz synchronous for every system (SMPTE 272M-1994).
  EXPECT_EQ(read_wav_header_rate(base + "_audio_00.wav"),
            orc::kAudioSampleRateHz);
  EXPECT_EQ(read_wav_header_rate(base + "_audio_01.wav"),
            orc::kAudioSampleRateHz);

  // --- Read back with the real cvbs_source stage ---
  orc::PALCVBSSourceStage source;
  const std::map<std::string, orc::ParameterValue> parameters{
      {"input_path", base + ".composite"}};
  ASSERT_TRUE(source.set_parameters(parameters));

  orc::ObservationContext observation_context;
  const auto outputs = source.execute({}, parameters, observation_context);
  ASSERT_EQ(outputs.size(), 1u);

  const auto read_vfr =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  ASSERT_NE(read_vfr, nullptr);
  EXPECT_EQ(read_vfr->frame_count(), kFrameCount);

  // --- Descriptors round-trip via the .meta audio_track table ---
  ASSERT_EQ(read_vfr->audio_channel_pair_count(), 2u);

  const auto pair0 = read_vfr->get_audio_channel_pair_descriptor(0);
  ASSERT_TRUE(pair0.has_value());
  EXPECT_EQ(pair0->name, "Analogue");
  // The container carries no origin metadata.
  EXPECT_EQ(pair0->origin, orc::AudioOrigin::UNKNOWN);

  const auto pair1 = read_vfr->get_audio_channel_pair_descriptor(1);
  ASSERT_TRUE(pair1.has_value());
  EXPECT_EQ(pair1->name, "EFM digital audio");
  EXPECT_EQ(pair1->origin, orc::AudioOrigin::UNKNOWN);

  // --- Samples round-trip per frame, sample-exact ---
  // The generators emit 24-bit values with a zero low byte, so the 16-bit
  // container narrow (>> 8) and read-back widen (<< 8) are lossless; a
  // 48000 Hz payload passes through the ingest conversion unresampled.
  for (orc::FrameID fid = 0; fid < kFrameCount; ++fid) {
    for (size_t pair = 0; pair < 2; ++pair) {
      const auto samples = read_vfr->get_audio_samples(pair, fid);
      ASSERT_EQ(samples.size(), static_cast<size_t>(kPalPairsPerFrame) * 2)
          << "pair " << pair << " frame " << fid;
      bool all_equal = true;
      for (size_t i = 0; i < samples.size(); ++i) {
        const int32_t expected =
            (pair == 0) ? pair0_sample(fid, i) : pair1_sample(fid, i);
        if (samples[i] != expected) {
          all_equal = false;
          break;
        }
      }
      EXPECT_TRUE(all_equal)
          << "samples differ in pair " << pair << " frame " << fid;
    }
  }

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

}  // namespace orc_functional_test
