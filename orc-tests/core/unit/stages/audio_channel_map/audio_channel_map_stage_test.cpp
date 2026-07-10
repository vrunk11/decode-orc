/*
 * File:        audio_channel_map_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for AudioChannelMapStage (dual-mono split, mono
 *              fill, channel swap on locked and free-running tracks)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/audio_channel_map/audio_channel_map_stage.h"

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

// Two-track source: locked "Analogue" track 0 and free-running "EFM" track 1.
std::shared_ptr<NiceMock<MockVideoFrameRepresentationArtifact>>
make_two_track_source() {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(2u));
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(
          orc::AudioTrackDescriptor{"Analogue", orc::AudioTrackOrigin::ANALOGUE,
                                    true, orc::AudioSampleRate{44100, 1}}));
  ON_CALL(*vfr, get_audio_track_descriptor(1))
      .WillByDefault(Return(orc::AudioTrackDescriptor{
          "EFM digital audio", orc::AudioTrackOrigin::EFM, false,
          orc::AudioSampleRate{44100, 1}}));
  // Track 0 per-frame samples: two pairs with distinct L/R values.
  ON_CALL(*vfr, get_audio_sample_count(0, orc::FrameID(0)))
      .WillByDefault(Return(2u));
  ON_CALL(*vfr, get_audio_samples(0, orc::FrameID(0)))
      .WillByDefault(Return(std::vector<int16_t>{1, 2, 3, 4}));
  // Track 1 stream: 100 pairs; reads return distinct L/R values.
  ON_CALL(*vfr, get_audio_stream_pair_count(1)).WillByDefault(Return(100u));
  ON_CALL(*vfr, get_audio_stream_samples(1, 10, 2))
      .WillByDefault(Return(std::vector<int16_t>{5, 6, 7, 8}));
  return vfr;
}

std::shared_ptr<const orc::VideoFrameRepresentation> run(
    orc::AudioChannelMapStage& stage, orc::ArtifactPtr input,
    const std::map<std::string, orc::ParameterValue>& params) {
  orc::ObservationContext ctx;
  auto outputs = stage.execute({std::move(input)}, params, ctx);
  EXPECT_EQ(outputs.size(), 1u);
  auto output = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      outputs[0]);
  EXPECT_NE(output, nullptr);
  return output;
}

TEST(AudioChannelMapStageTest, NodeTypeInfo_ReportsTransformWithOneInput) {
  orc::AudioChannelMapStage stage;
  const auto info = stage.get_node_type_info();

  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "audio_channel_map");
  EXPECT_EQ(stage.required_input_count(), 1u);
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(AudioChannelMapStageTest, Descriptors_DefaultsRoundTripThroughSetGet) {
  orc::AudioChannelMapStage stage;
  const auto descriptors = stage.get_parameter_descriptors();

  const auto* track = find_descriptor(descriptors, "track");
  const auto* operation = find_descriptor(descriptors, "operation");
  ASSERT_NE(track, nullptr);
  ASSERT_NE(operation, nullptr);
  EXPECT_EQ(track->type, orc::ParameterType::INT32);
  EXPECT_EQ(operation->type, orc::ParameterType::STRING);
  EXPECT_EQ(std::get<int32_t>(*track->constraints.default_value), 0);
  EXPECT_EQ(std::get<std::string>(*operation->constraints.default_value),
            "split_dual_mono");
  EXPECT_EQ(operation->constraints.allowed_strings.size(), 4u);

  auto params = stage.get_parameters();
  EXPECT_EQ(std::get<int32_t>(params.at("track")), 0);
  EXPECT_EQ(std::get<std::string>(params.at("operation")), "split_dual_mono");

  EXPECT_TRUE(stage.set_parameters(
      {{"track", int32_t{3}}, {"operation", std::string("swap_channels")}}));
  params = stage.get_parameters();
  EXPECT_EQ(std::get<int32_t>(params.at("track")), 3);
  EXPECT_EQ(std::get<std::string>(params.at("operation")), "swap_channels");
}

TEST(AudioChannelMapStageTest, SetParameters_RejectsInvalidValues) {
  orc::AudioChannelMapStage stage;
  EXPECT_FALSE(stage.set_parameters({{"track", int32_t{-1}}}));
  EXPECT_FALSE(stage.set_parameters({{"track", int32_t{16}}}));
  EXPECT_FALSE(stage.set_parameters({{"operation", std::string("bogus")}}));
}

TEST(AudioChannelMapStageTest, Execute_ThrowsOnMissingInput) {
  orc::AudioChannelMapStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, Execute_ThrowsWhenTrackOutOfRange) {
  orc::AudioChannelMapStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({vfr}, {{"track", int32_t{1}}}, ctx),
               orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, Execute_ThrowsWhenSplitWouldExceedTrackCap) {
  orc::AudioChannelMapStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count())
      .WillByDefault(Return(orc::kMaxAudioTracks));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute(
                   {vfr}, {{"operation", std::string("split_dual_mono")}}, ctx),
               orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, SplitDualMono_ReplacesInPlaceAndAppends) {
  orc::AudioChannelMapStage stage;
  auto vfr = make_two_track_source();
  auto output = run(
      stage, vfr,
      {{"track", int32_t{0}}, {"operation", std::string("split_dual_mono")}});

  ASSERT_EQ(output->audio_track_count(), 3u);

  // Target index becomes the left-channel mono track, in place.
  const auto left = output->get_audio_track_descriptor(0);
  ASSERT_TRUE(left.has_value());
  EXPECT_EQ(left->name, "Analogue (L)");
  EXPECT_EQ(left->origin, orc::AudioTrackOrigin::DERIVED);
  EXPECT_TRUE(left->locked);

  // Non-targeted track keeps its index and descriptor.
  const auto other = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(other.has_value());
  EXPECT_EQ(other->name, "EFM digital audio");
  EXPECT_EQ(other->origin, orc::AudioTrackOrigin::EFM);

  // The right-channel mono track is appended last.
  const auto right = output->get_audio_track_descriptor(2);
  ASSERT_TRUE(right.has_value());
  EXPECT_EQ(right->name, "Analogue (R)");
  EXPECT_EQ(right->origin, orc::AudioTrackOrigin::DERIVED);
  EXPECT_TRUE(right->locked);

  EXPECT_FALSE(output->get_audio_track_descriptor(3).has_value());

  // Source pairs {L=1,R=2},{L=3,R=4}: (L) carries L on both channels, (R)
  // carries R on both channels; the appended track mirrors the target's
  // per-frame layout.
  EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(0)),
            (std::vector<int16_t>{1, 1, 3, 3}));
  EXPECT_EQ(output->get_audio_samples(2, orc::FrameID(0)),
            (std::vector<int16_t>{2, 2, 4, 4}));
  EXPECT_EQ(output->get_audio_sample_count(2, orc::FrameID(0)), 2u);
}

TEST(AudioChannelMapStageTest, SplitDualMono_WorksOnFreeRunningStreams) {
  orc::AudioChannelMapStage stage;
  auto vfr = make_two_track_source();
  auto output = run(
      stage, vfr,
      {{"track", int32_t{1}}, {"operation", std::string("split_dual_mono")}});

  ASSERT_EQ(output->audio_track_count(), 3u);
  EXPECT_EQ(output->get_audio_stream_pair_count(1), 100u);
  EXPECT_EQ(output->get_audio_stream_pair_count(2), 100u);

  // Source pairs {L=5,R=6},{L=7,R=8}.
  EXPECT_EQ(output->get_audio_stream_samples(1, 10, 2),
            (std::vector<int16_t>{5, 5, 7, 7}));
  EXPECT_EQ(output->get_audio_stream_samples(2, 10, 2),
            (std::vector<int16_t>{6, 6, 8, 8}));
}

TEST(AudioChannelMapStageTest, InPlaceOperations_RemapChannels) {
  const std::vector<std::pair<std::string, std::vector<int16_t>>> cases = {
      {"left_to_both", {1, 1, 3, 3}},
      {"right_to_both", {2, 2, 4, 4}},
      {"swap_channels", {2, 1, 4, 3}},
  };

  for (const auto& [operation, expected] : cases) {
    orc::AudioChannelMapStage stage;
    auto vfr = make_two_track_source();
    auto output =
        run(stage, vfr, {{"track", int32_t{0}}, {"operation", operation}});

    // In-place: no track added, descriptor unchanged.
    EXPECT_EQ(output->audio_track_count(), 2u) << operation;
    const auto desc = output->get_audio_track_descriptor(0);
    ASSERT_TRUE(desc.has_value()) << operation;
    EXPECT_EQ(desc->name, "Analogue") << operation;
    EXPECT_EQ(desc->origin, orc::AudioTrackOrigin::ANALOGUE) << operation;

    EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(0)), expected)
        << operation;
    // Non-targeted track untouched.
    EXPECT_EQ(output->get_audio_stream_samples(1, 10, 2),
              (std::vector<int16_t>{5, 6, 7, 8}))
        << operation;
  }
}

}  // namespace
}  // namespace orc_unit_test
