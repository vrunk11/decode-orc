/*
 * File:        audio_channel_map_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for AudioChannelMapStage (dual-mono split, mono
 *              fill, channel swap on synchronous channel pairs)
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

// Two-pair source: "Analogue" pair 0 and "EFM digital audio" pair 1, each
// carrying two stereo pairs with distinct 24-bit L/R values for frame 0.
std::shared_ptr<NiceMock<MockVideoFrameRepresentationArtifact>>
make_two_pair_source() {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_channel_pair_count()).WillByDefault(Return(2u));
  ON_CALL(*vfr, get_audio_channel_pair_descriptor(0))
      .WillByDefault(Return(orc::AudioChannelPairDescriptor{
          "Analogue", orc::AudioOrigin::ANALOGUE}));
  ON_CALL(*vfr, get_audio_channel_pair_descriptor(1))
      .WillByDefault(Return(orc::AudioChannelPairDescriptor{
          "EFM digital audio", orc::AudioOrigin::EFM}));
  ON_CALL(*vfr, get_audio_samples(0, orc::FrameID(0)))
      .WillByDefault(
          Return(std::vector<int32_t>{100000, 200000, -300000, 400000}));
  ON_CALL(*vfr, get_audio_samples(1, orc::FrameID(0)))
      .WillByDefault(Return(std::vector<int32_t>{5000, 6000, 7000, 8000}));
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

  const auto* channel_pair = find_descriptor(descriptors, "channel_pair");
  const auto* operation = find_descriptor(descriptors, "operation");
  ASSERT_NE(channel_pair, nullptr);
  ASSERT_NE(operation, nullptr);
  EXPECT_EQ(channel_pair->type, orc::ParameterType::INT32);
  EXPECT_EQ(operation->type, orc::ParameterType::STRING);
  EXPECT_EQ(std::get<int32_t>(*channel_pair->constraints.default_value), 0);
  EXPECT_EQ(std::get<int32_t>(*channel_pair->constraints.max_value),
            static_cast<int32_t>(orc::kMaxAudioChannelPairs) - 1);
  EXPECT_EQ(std::get<std::string>(*operation->constraints.default_value),
            "split_dual_mono");
  EXPECT_EQ(operation->constraints.allowed_strings.size(), 4u);

  auto params = stage.get_parameters();
  EXPECT_EQ(std::get<int32_t>(params.at("channel_pair")), 0);
  EXPECT_EQ(std::get<std::string>(params.at("operation")), "split_dual_mono");

  EXPECT_TRUE(
      stage.set_parameters({{"channel_pair", int32_t{3}},
                            {"operation", std::string("swap_channels")}}));
  params = stage.get_parameters();
  EXPECT_EQ(std::get<int32_t>(params.at("channel_pair")), 3);
  EXPECT_EQ(std::get<std::string>(params.at("operation")), "swap_channels");
}

TEST(AudioChannelMapStageTest, SetParameters_RejectsInvalidValues) {
  orc::AudioChannelMapStage stage;
  EXPECT_FALSE(stage.set_parameters({{"channel_pair", int32_t{-1}}}));
  EXPECT_FALSE(stage.set_parameters(
      {{"channel_pair", static_cast<int32_t>(orc::kMaxAudioChannelPairs)}}));
  EXPECT_FALSE(stage.set_parameters({{"operation", std::string("bogus")}}));
}

TEST(AudioChannelMapStageTest, Execute_ThrowsOnMissingInput) {
  orc::AudioChannelMapStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, Execute_ThrowsWhenChannelPairOutOfRange) {
  orc::AudioChannelMapStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_channel_pair_count()).WillByDefault(Return(1u));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({vfr}, {{"channel_pair", int32_t{1}}}, ctx),
               orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, Execute_ThrowsWhenSplitWouldExceedPairCap) {
  orc::AudioChannelMapStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_channel_pair_count())
      .WillByDefault(Return(orc::kMaxAudioChannelPairs));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute(
                   {vfr}, {{"operation", std::string("split_dual_mono")}}, ctx),
               orc::DAGExecutionError);
}

TEST(AudioChannelMapStageTest, SplitDualMono_ReplacesInPlaceAndAppends) {
  orc::AudioChannelMapStage stage;
  auto vfr = make_two_pair_source();
  auto output = run(stage, vfr,
                    {{"channel_pair", int32_t{0}},
                     {"operation", std::string("split_dual_mono")}});

  ASSERT_EQ(output->audio_channel_pair_count(), 3u);

  // Target index becomes the left-channel mono pair, in place.
  const auto left = output->get_audio_channel_pair_descriptor(0);
  ASSERT_TRUE(left.has_value());
  EXPECT_EQ(left->name, "Analogue (L)");
  EXPECT_EQ(left->origin, orc::AudioOrigin::DERIVED);

  // Non-targeted pair keeps its index and descriptor.
  const auto other = output->get_audio_channel_pair_descriptor(1);
  ASSERT_TRUE(other.has_value());
  EXPECT_EQ(other->name, "EFM digital audio");
  EXPECT_EQ(other->origin, orc::AudioOrigin::EFM);

  // The right-channel mono pair is appended last.
  const auto right = output->get_audio_channel_pair_descriptor(2);
  ASSERT_TRUE(right.has_value());
  EXPECT_EQ(right->name, "Analogue (R)");
  EXPECT_EQ(right->origin, orc::AudioOrigin::DERIVED);

  EXPECT_FALSE(output->get_audio_channel_pair_descriptor(3).has_value());

  // Source pairs {L=100000,R=200000},{L=-300000,R=400000}: (L) carries L on
  // both channels, (R) carries R on both channels; the appended pair mirrors
  // the target's per-frame layout.
  EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(0)),
            (std::vector<int32_t>{100000, 100000, -300000, -300000}));
  EXPECT_EQ(output->get_audio_samples(2, orc::FrameID(0)),
            (std::vector<int32_t>{200000, 200000, 400000, 400000}));
}

TEST(AudioChannelMapStageTest, OutOfRangePair_ReturnsEmptyAndNullopt) {
  orc::AudioChannelMapStage stage;
  auto vfr = make_two_pair_source();
  auto output = run(stage, vfr,
                    {{"channel_pair", int32_t{0}},
                     {"operation", std::string("split_dual_mono")}});

  // Split output carries pairs 0-2; pair 3 is out of range.
  EXPECT_TRUE(output->get_audio_samples(3, orc::FrameID(0)).empty());
  EXPECT_FALSE(output->get_audio_channel_pair_descriptor(3).has_value());
}

TEST(AudioChannelMapStageTest, InPlaceOperations_RemapChannels) {
  const std::vector<std::pair<std::string, std::vector<int32_t>>> cases = {
      {"left_to_both", {100000, 100000, -300000, -300000}},
      {"right_to_both", {200000, 200000, 400000, 400000}},
      {"swap_channels", {200000, 100000, 400000, -300000}},
  };

  for (const auto& [operation, expected] : cases) {
    orc::AudioChannelMapStage stage;
    auto vfr = make_two_pair_source();
    auto output = run(stage, vfr,
                      {{"channel_pair", int32_t{0}}, {"operation", operation}});

    // In-place: no pair added, descriptor unchanged.
    EXPECT_EQ(output->audio_channel_pair_count(), 2u) << operation;
    const auto desc = output->get_audio_channel_pair_descriptor(0);
    ASSERT_TRUE(desc.has_value()) << operation;
    EXPECT_EQ(desc->name, "Analogue") << operation;
    EXPECT_EQ(desc->origin, orc::AudioOrigin::ANALOGUE) << operation;

    EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(0)), expected)
        << operation;
    // Non-targeted pair untouched.
    EXPECT_EQ(output->get_audio_samples(1, orc::FrameID(0)),
              (std::vector<int32_t>{5000, 6000, 7000, 8000}))
        << operation;
  }
}

}  // namespace
}  // namespace orc_unit_test
