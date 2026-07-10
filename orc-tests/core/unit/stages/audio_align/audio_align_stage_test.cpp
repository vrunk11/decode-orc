/*
 * File:        audio_align_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for AudioAlignStage (locked window assembly with
 *              edge silence, free-running origin shift, offset conversion)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/audio_align/audio_align_stage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <orc/stage/observation_context.h>

#include <algorithm>

#include "../../include/video_frame_representation_artifact_mock.h"

namespace orc_unit_test {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

const orc::ParameterDescriptor* find_descriptor(
    const std::vector<orc::ParameterDescriptor>& descs,
    const std::string& name) {
  auto it = std::find_if(
      descs.begin(), descs.end(),
      [&](const orc::ParameterDescriptor& d) { return d.name == name; });
  return it == descs.end() ? nullptr : &(*it);
}

// Locked source with 3 frames of 4 pairs each; pair p of the track carries
// the value 100 + global_pair_index on both channels.
std::shared_ptr<NiceMock<MockVideoFrameRepresentationArtifact>>
make_locked_source() {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(
          orc::AudioTrackDescriptor{"Analogue", orc::AudioTrackOrigin::ANALOGUE,
                                    true, orc::AudioSampleRate{1000, 1}}));
  ON_CALL(*vfr, frame_range())
      .WillByDefault(
          Return(orc::FrameIDRange{orc::FrameID(0), orc::FrameID(2)}));
  for (uint64_t frame = 0; frame < 3; ++frame) {
    ON_CALL(*vfr, get_audio_sample_count(0, orc::FrameID(frame)))
        .WillByDefault(Return(4u));
    std::vector<int16_t> samples;
    for (uint64_t pair = 0; pair < 4; ++pair) {
      const auto value = static_cast<int16_t>(100 + frame * 4 + pair);
      samples.push_back(value);
      samples.push_back(value);
    }
    ON_CALL(*vfr, get_audio_samples(0, orc::FrameID(frame)))
        .WillByDefault(Return(samples));
  }
  return vfr;
}

TEST(AudioAlignStageTest, NodeTypeInfo_ReportsTransformWithOneInput) {
  orc::AudioAlignStage stage;
  const auto info = stage.get_node_type_info();

  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "audio_align");
  EXPECT_EQ(stage.required_input_count(), 1u);
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(AudioAlignStageTest, Descriptors_DefaultsRoundTripThroughSetGet) {
  orc::AudioAlignStage stage;
  const auto descriptors = stage.get_parameter_descriptors();

  const auto* track = find_descriptor(descriptors, "track");
  const auto* offset = find_descriptor(descriptors, "offset_ms");
  ASSERT_NE(track, nullptr);
  ASSERT_NE(offset, nullptr);
  EXPECT_EQ(track->type, orc::ParameterType::INT32);
  EXPECT_EQ(offset->type, orc::ParameterType::DOUBLE);
  EXPECT_EQ(std::get<int32_t>(*track->constraints.default_value), 0);
  EXPECT_EQ(std::get<double>(*offset->constraints.default_value), 0.0);

  EXPECT_TRUE(
      stage.set_parameters({{"track", int32_t{2}}, {"offset_ms", -12.5}}));
  const auto params = stage.get_parameters();
  EXPECT_EQ(std::get<int32_t>(params.at("track")), 2);
  EXPECT_EQ(std::get<double>(params.at("offset_ms")), -12.5);
}

TEST(AudioAlignStageTest, Execute_ThrowsWhenTrackOutOfRange) {
  orc::AudioAlignStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({vfr}, {{"track", int32_t{1}}}, ctx),
               orc::DAGExecutionError);
}

TEST(AudioAlignStageTest, Execute_ZeroOffsetPassesInputThrough) {
  orc::AudioAlignStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));

  orc::ObservationContext ctx;
  auto outputs = stage.execute({vfr}, {{"offset_ms", 0.0}}, ctx);
  ASSERT_EQ(outputs.size(), 1u);
  EXPECT_EQ(outputs[0].get(), vfr.get());
}

TEST(AudioAlignStageTest, LockedPositiveOffset_DelaysWithLeadInSilence) {
  // +2 pairs at 1000 Hz = 2 ms.
  auto vfr = make_locked_source();
  const orc::AlignedAudioTrackRepresentation aligned(vfr, 0, 2);

  // Frame 0 window reads source positions [-2, 2): two silence pairs then
  // the first two source pairs.
  EXPECT_EQ(aligned.get_audio_samples(0, orc::FrameID(0)),
            (std::vector<int16_t>{0, 0, 0, 0, 100, 100, 101, 101}));
  // Frame 1 window reads positions [2, 6), spanning frames 0 and 1.
  EXPECT_EQ(aligned.get_audio_samples(0, orc::FrameID(1)),
            (std::vector<int16_t>{102, 102, 103, 103, 104, 104, 105, 105}));
  // Per-frame pair counts are unchanged.
  EXPECT_EQ(aligned.get_audio_sample_count(0, orc::FrameID(0)), 4u);
}

TEST(AudioAlignStageTest, LockedNegativeOffset_AdvancesWithTailSilence) {
  // -2 pairs: frame windows read two pairs ahead.
  auto vfr = make_locked_source();
  const orc::AlignedAudioTrackRepresentation aligned(vfr, 0, -2);

  EXPECT_EQ(aligned.get_audio_samples(0, orc::FrameID(0)),
            (std::vector<int16_t>{102, 102, 103, 103, 104, 104, 105, 105}));
  // The last frame's window reads past the end of the range: silence tail.
  EXPECT_EQ(aligned.get_audio_samples(0, orc::FrameID(2)),
            (std::vector<int16_t>{110, 110, 111, 111, 0, 0, 0, 0}));
}

TEST(AudioAlignStageTest, FreeRunningPositiveOffset_PrependsSilence) {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(orc::AudioTrackDescriptor{
          "EFM digital audio", orc::AudioTrackOrigin::EFM, false,
          orc::AudioSampleRate{44100, 1}}));
  ON_CALL(*vfr, get_audio_stream_pair_count(0)).WillByDefault(Return(10u));
  ON_CALL(*vfr, get_audio_stream_samples(0, 0, 3))
      .WillByDefault(Return(std::vector<int16_t>{1, 1, 2, 2, 3, 3}));

  const orc::AlignedAudioTrackRepresentation aligned(vfr, 0, 3);

  EXPECT_EQ(aligned.get_audio_stream_pair_count(0), 13u);
  // Pairs [1, 6): two remaining silence pairs then source pairs [0, 3).
  EXPECT_EQ(aligned.get_audio_stream_samples(0, 1, 5),
            (std::vector<int16_t>{0, 0, 0, 0, 1, 1, 2, 2, 3, 3}));
  // Locked accessors stay empty for a free-running track.
  EXPECT_TRUE(aligned.get_audio_samples(0, orc::FrameID(0)).empty());
}

TEST(AudioAlignStageTest, FreeRunningNegativeOffset_TrimsStreamStart) {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(orc::AudioTrackDescriptor{
          "EFM digital audio", orc::AudioTrackOrigin::EFM, false,
          orc::AudioSampleRate{44100, 1}}));
  ON_CALL(*vfr, get_audio_stream_pair_count(0)).WillByDefault(Return(10u));
  ON_CALL(*vfr, get_audio_stream_samples(0, 5, 4))
      .WillByDefault(
          Return(std::vector<int16_t>{9, 9, 10, 10, 11, 11, 12, 12}));

  const orc::AlignedAudioTrackRepresentation aligned(vfr, 0, -3);

  EXPECT_EQ(aligned.get_audio_stream_pair_count(0), 7u);
  EXPECT_EQ(aligned.get_audio_stream_samples(0, 2, 4),
            (std::vector<int16_t>{9, 9, 10, 10, 11, 11, 12, 12}));
}

TEST(AudioAlignStageTest, NonTargetTracks_ForwardUntouched) {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(2u));
  ON_CALL(*vfr, get_audio_track_descriptor(1))
      .WillByDefault(Return(
          orc::AudioTrackDescriptor{"Other", orc::AudioTrackOrigin::UNKNOWN,
                                    false, orc::AudioSampleRate{44100, 1}}));
  ON_CALL(*vfr, get_audio_stream_pair_count(1)).WillByDefault(Return(50u));
  ON_CALL(*vfr, get_audio_stream_samples(1, 7, 2))
      .WillByDefault(Return(std::vector<int16_t>{4, 5, 6, 7}));

  const orc::AlignedAudioTrackRepresentation aligned(vfr, 0, 100);

  EXPECT_EQ(aligned.get_audio_stream_pair_count(1), 50u);
  EXPECT_EQ(aligned.get_audio_stream_samples(1, 7, 2),
            (std::vector<int16_t>{4, 5, 6, 7}));
}

TEST(AudioAlignStageTest, Execute_ConvertsMillisecondsAtTrackRate) {
  // 3 ms at 1000 pairs/s = 3 pairs of lead-in on a free-running track.
  orc::AudioAlignStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(
          orc::AudioTrackDescriptor{"Track", orc::AudioTrackOrigin::UNKNOWN,
                                    false, orc::AudioSampleRate{1000, 1}}));
  ON_CALL(*vfr, get_audio_stream_pair_count(0)).WillByDefault(Return(10u));

  orc::ObservationContext ctx;
  auto outputs = stage.execute({vfr}, {{"offset_ms", 3.0}}, ctx);
  ASSERT_EQ(outputs.size(), 1u);
  auto output = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      outputs[0]);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(output->get_audio_stream_pair_count(0), 13u);
}

}  // namespace
}  // namespace orc_unit_test
