/*
 * File:        audio_sink_stage_deps_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for audio sink stage dependencies
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_sink_stage_deps.h"

#include <gtest/gtest.h>
#include <orc/stage/audio_channel_pair.h>

#include <cstring>

#include "../../include/video_frame_representation_artifact_mock.h"
#include "../../stage_services_mock.h"

using testing::A;
using testing::Return;
using testing::StrictMock;

namespace orc_unit_test {

namespace {

orc::SourceParameters make_system_params(orc::VideoSystem system) {
  orc::SourceParameters params;
  params.system = system;
  return params;
}

std::optional<orc::AudioChannelPairDescriptor> analogue_pair_descriptor() {
  orc::AudioChannelPairDescriptor desc;
  desc.name = "Analogue";
  desc.origin = orc::AudioOrigin::ANALOGUE;
  return desc;
}

// Builds a cadence-sized frame of interleaved int32 carrier samples whose
// leading values follow |leading| and whose remainder is zero.
std::vector<int32_t> make_frame_samples(uint64_t frame_index,
                                        orc::VideoSystem system,
                                        const std::vector<int32_t>& leading) {
  std::vector<int32_t> samples(
      static_cast<size_t>(orc::audio_pairs_in_frame(frame_index, system)) * 2,
      0);
  for (size_t i = 0; i < leading.size() && i < samples.size(); ++i) {
    samples[i] = leading[i];
  }
  return samples;
}

// Reads a little-endian uint32 at the given byte offset from the int16-word
// stream the deps use to write the RIFF header.
uint32_t header_u32_at(const std::vector<int16_t>& words, size_t byte_offset) {
  uint32_t value = 0;
  std::memcpy(&value,
              reinterpret_cast<const uint8_t*>(words.data()) + byte_offset, 4);
  return value;
}

constexpr size_t kWavSampleRateOffset = 24;
constexpr size_t kWavDataSizeOffset = 40;

}  // namespace

class AudioSinkStageDeps : public ::testing::Test {
 public:
  void SetUp() override {
    pMockFileWriterInt16_ = std::make_shared<StrictMock<MockFileWriterInt16>>();

    instance_ = std::make_unique<orc::AudioSinkStageDeps>(&mockStageServices_);
    instance_->init({}, &isProcessing_, &cancelRequested_);

    cancelRequested_.store(false);
    isProcessing_.store(true);
  }

 protected:
  void expect_writer_created_and_opened() {
    EXPECT_CALL(mockStageServices_,
                create_buffered_file_writer_int16(4UL * 1024 * 1024))
        .Times(1)
        .WillOnce(Return(pMockFileWriterInt16_));
    EXPECT_CALL(*pMockFileWriterInt16_, open("out_path.wav"))
        .Times(1)
        .WillOnce(Return(true));
  }

  void capture_writes(std::vector<std::vector<int16_t>>& writes, int count) {
    EXPECT_CALL(*pMockFileWriterInt16_, write(A<const std::vector<int16_t>&>()))
        .Times(count)
        .WillRepeatedly([&writes](const std::vector<int16_t>& data) {
          writes.push_back(data);
        });
    EXPECT_CALL(*pMockFileWriterInt16_, close()).Times(1);
  }

  MockStageServices mockStageServices_;
  std::shared_ptr<StrictMock<MockFileWriterInt16>> pMockFileWriterInt16_;
  StrictMock<MockVideoFrameRepresentationArtifact> mockRepresentation_;

  std::atomic<bool> cancelRequested_{};
  std::atomic<bool> isProcessing_{};
  std::unique_ptr<orc::AudioSinkStageDeps> instance_;
};

TEST_F(AudioSinkStageDeps,
       WriteAudioWav_Declares48kHzAndNarrowsCarrierTo16Bit) {
  // One PAL frame: 1920 stereo pairs (SMPTE 272M-1994, 48000 / 25).
  constexpr uint32_t kPalPairsPerFrame = 1920;
  // 24-bit-in-int32 carrier values whose >> 8 narrowing is {1, -2, 3, -4}.
  const std::vector<int32_t> carrier =
      make_frame_samples(0, orc::VideoSystem::PAL, {256, -512, 768, -1024});

  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(0))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(make_system_params(orc::VideoSystem::PAL)));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));
  EXPECT_CALL(mockRepresentation_, get_audio_samples(0, 0))
      .Times(1)
      .WillOnce(Return(carrier));

  expect_writer_created_and_opened();

  // One write for the 44-byte RIFF header (22 int16 words) and one for the
  // frame payload.
  std::vector<std::vector<int16_t>> writes;
  capture_writes(writes, 2);

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 0);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.frames_written, kPalPairsPerFrame);
  ASSERT_EQ(writes.size(), 2U);
  // SMPTE 272M-1994 §1.2: 48 kHz for every video system.
  EXPECT_EQ(header_u32_at(writes[0], kWavSampleRateOffset), 48000U);
  // Declared payload: 1920 stereo pairs × 4 bytes.
  EXPECT_EQ(header_u32_at(writes[0], kWavDataSizeOffset),
            kPalPairsPerFrame * 4U);
  // Payload is the >> 8 narrowing of the int32 carrier.
  ASSERT_EQ(writes[1].size(), static_cast<size_t>(kPalPairsPerFrame) * 2);
  EXPECT_EQ(writes[1][0], 1);
  EXPECT_EQ(writes[1][1], -2);
  EXPECT_EQ(writes[1][2], 3);
  EXPECT_EQ(writes[1][3], -4);
  EXPECT_EQ(writes[1][4], 0);
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_SilenceFramesAreSizedByNtscCadence) {
  // Two NTSC frames with no audio: silence must follow the 5-frame audio
  // frame sequence (SMPTE 272M-1994 §14.3 Table 1: frame 0 = 1602 pairs,
  // frame 1 = 1601 pairs).
  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(0))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(make_system_params(orc::VideoSystem::NTSC)));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 1}));
  EXPECT_CALL(mockRepresentation_, get_audio_samples(0, 0))
      .Times(1)
      .WillOnce(Return(std::vector<int32_t>{}));
  EXPECT_CALL(mockRepresentation_, get_audio_samples(0, 1))
      .Times(1)
      .WillOnce(Return(std::vector<int32_t>{}));

  expect_writer_created_and_opened();

  // Header plus one silence write per frame.
  std::vector<std::vector<int16_t>> writes;
  capture_writes(writes, 3);

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 0);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.frames_written, 1602U + 1601U);
  ASSERT_EQ(writes.size(), 3U);
  EXPECT_EQ(header_u32_at(writes[0], kWavSampleRateOffset), 48000U);
  EXPECT_EQ(header_u32_at(writes[0], kWavDataSizeOffset), (1602U + 1601U) * 4U);
  EXPECT_EQ(writes[1], std::vector<int16_t>(1602 * 2, 0));
  EXPECT_EQ(writes[2], std::vector<int16_t>(1601 * 2, 0));
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_SelectedPair_ReadsThatPairOnly) {
  const std::vector<int32_t> carrier =
      make_frame_samples(0, orc::VideoSystem::PAL, {1792, -1792});

  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(1))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(make_system_params(orc::VideoSystem::PAL)));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));
  EXPECT_CALL(mockRepresentation_, get_audio_samples(1, 0))
      .Times(1)
      .WillOnce(Return(carrier));

  expect_writer_created_and_opened();

  std::vector<std::vector<int16_t>> writes;
  capture_writes(writes, 2);

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 1);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.frames_written, 1920U);
  ASSERT_EQ(writes.size(), 2U);
  EXPECT_EQ(writes[1][0], 7);
  EXPECT_EQ(writes[1][1], -7);
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_FailsWhenChannelPairDoesNotExist) {
  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(3))
      .Times(1)
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mockRepresentation_, audio_channel_pair_count())
      .Times(1)
      .WillOnce(Return(1));

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 3);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message,
            "Audio channel pair 3 does not exist in the input (1 channel "
            "pair(s) available)");
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_FailsWhenVideoSystemHasNoAudioLayout) {
  // Without a known video system there is no defined audio frame sequence,
  // so the cadence-derived payload is empty and the write fails.
  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(0))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 0);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message, "No audio samples found in frame range");
}

TEST_F(AudioSinkStageDeps,
       WriteAudioWav_FailsWithDiagnostic_WhenWriterServiceUnavailable) {
  orc::AudioSinkStageDeps deps_without_services(nullptr);
  deps_without_services.init({}, &isProcessing_, &cancelRequested_);

  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(0))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(make_system_params(orc::VideoSystem::PAL)));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));

  const auto result = deps_without_services.write_audio_wav(
      &mockRepresentation_, "out_path.wav", 0);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message, "File writer service unavailable");
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_Fails_WhenWriterCannotOpenFile) {
  EXPECT_CALL(mockRepresentation_, get_audio_channel_pair_descriptor(0))
      .Times(1)
      .WillOnce(Return(analogue_pair_descriptor()));
  EXPECT_CALL(mockRepresentation_, get_video_parameters())
      .Times(1)
      .WillOnce(Return(make_system_params(orc::VideoSystem::PAL)));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));

  EXPECT_CALL(mockStageServices_,
              create_buffered_file_writer_int16(4UL * 1024 * 1024))
      .Times(1)
      .WillOnce(Return(pMockFileWriterInt16_));
  EXPECT_CALL(*pMockFileWriterInt16_, open("out_path.wav"))
      .Times(1)
      .WillOnce(Return(false));

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav", 0);

  EXPECT_FALSE(result.success);
}

}  // namespace orc_unit_test
