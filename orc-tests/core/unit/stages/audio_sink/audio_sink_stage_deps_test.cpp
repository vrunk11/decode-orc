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

#include "../../include/video_frame_representation_artifact_mock.h"
#include "../../stage_services_mock.h"

using testing::Return;
using testing::StrictMock;

namespace orc_unit_test {

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
  MockStageServices mockStageServices_;
  std::shared_ptr<StrictMock<MockFileWriterInt16>> pMockFileWriterInt16_;
  StrictMock<MockVideoFrameRepresentationArtifact> mockRepresentation_;

  std::atomic<bool> cancelRequested_{};
  std::atomic<bool> isProcessing_{};
  std::unique_ptr<orc::AudioSinkStageDeps> instance_;
};

TEST_F(AudioSinkStageDeps,
       WriteAudioWav_StreamsHeaderAndSamplesThroughInt16WriterService) {
  EXPECT_CALL(mockRepresentation_, audio_locked())
      .Times(1)
      .WillOnce(Return(true));
  // Single-frame range with four locked stereo samples.
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));
  EXPECT_CALL(mockRepresentation_, get_audio_sample_count(0))
      .Times(1)
      .WillOnce(Return(4));
  EXPECT_CALL(mockRepresentation_, get_audio_samples(0))
      .Times(1)
      .WillOnce(Return(std::vector<int16_t>{1, -2, 3, -4}));

  EXPECT_CALL(mockStageServices_,
              create_buffered_file_writer_int16(4UL * 1024 * 1024))
      .Times(1)
      .WillOnce(Return(pMockFileWriterInt16_));

  EXPECT_CALL(*pMockFileWriterInt16_, open("out_path.wav"))
      .Times(1)
      .WillOnce(Return(true));

  // One write for the 44-byte RIFF header (22 int16 words) and one for the
  // sample payload.
  EXPECT_CALL(*pMockFileWriterInt16_,
              write(testing::A<const std::vector<int16_t>&>()))
      .Times(2);

  EXPECT_CALL(*pMockFileWriterInt16_, close()).Times(1);

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav");

  EXPECT_TRUE(result.success);
  // Four interleaved stereo samples = two audio frames.
  EXPECT_EQ(result.frames_written, 2U);
}

TEST_F(AudioSinkStageDeps,
       WriteAudioWav_FailsWithDiagnostic_WhenWriterServiceUnavailable) {
  orc::AudioSinkStageDeps deps_without_services(nullptr);
  deps_without_services.init({}, &isProcessing_, &cancelRequested_);

  EXPECT_CALL(mockRepresentation_, audio_locked())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));
  EXPECT_CALL(mockRepresentation_, get_audio_sample_count(0))
      .Times(1)
      .WillOnce(Return(4));

  const auto result = deps_without_services.write_audio_wav(
      &mockRepresentation_, "out_path.wav");

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_message, "File writer service unavailable");
}

TEST_F(AudioSinkStageDeps, WriteAudioWav_Fails_WhenWriterCannotOpenFile) {
  EXPECT_CALL(mockRepresentation_, audio_locked())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mockRepresentation_, frame_range())
      .Times(1)
      .WillOnce(Return(orc::FrameIDRange{0, 0}));
  EXPECT_CALL(mockRepresentation_, get_audio_sample_count(0))
      .Times(1)
      .WillOnce(Return(4));

  EXPECT_CALL(mockStageServices_,
              create_buffered_file_writer_int16(4UL * 1024 * 1024))
      .Times(1)
      .WillOnce(Return(pMockFileWriterInt16_));
  EXPECT_CALL(*pMockFileWriterInt16_, open("out_path.wav"))
      .Times(1)
      .WillOnce(Return(false));

  const auto result =
      instance_->write_audio_wav(&mockRepresentation_, "out_path.wav");

  EXPECT_FALSE(result.success);
}

}  // namespace orc_unit_test
