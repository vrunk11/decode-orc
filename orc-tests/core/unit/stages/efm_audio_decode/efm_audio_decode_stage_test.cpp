/*
 * File:        efm_audio_decode_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for EFMAudioDecodeStage (lazy EFM decode, appended
 *              free-running track, pass-through forwarding)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/efm_audio_decode/efm_audio_decode_stage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <orc/stage/observation_context.h>

#include <algorithm>

#include "../../include/video_frame_representation_artifact_mock.h"

namespace orc_unit_test {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

class MockEFMAudioDecodeDeps : public orc::IEFMAudioDecodeDeps {
 public:
  MOCK_METHOD(orc::EFMAudioDecodeResult, decode_to_cache,
              (const orc::VideoFrameRepresentation& representation,
               const orc::EFMAudioDecodeOptions& options),
              (override));
  MOCK_METHOD((std::vector<int16_t>), read_cache_pairs,
              (uint64_t first_pair, uint32_t pair_count), (const, override));
};

const orc::ParameterDescriptor* find_descriptor(
    const std::vector<orc::ParameterDescriptor>& descs,
    const std::string& name) {
  auto it = std::find_if(
      descs.begin(), descs.end(),
      [&](const orc::ParameterDescriptor& d) { return d.name == name; });
  return it == descs.end() ? nullptr : &(*it);
}

// Runs the stage over a mock source with |source_tracks| existing audio
// tracks and returns the output representation.
std::shared_ptr<const orc::VideoFrameRepresentation> make_output(
    orc::EFMAudioDecodeStage& stage,
    std::shared_ptr<NiceMock<MockVideoFrameRepresentationArtifact>> vfr,
    size_t source_tracks) {
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(source_tracks));
  orc::ObservationContext ctx;
  auto outputs = stage.execute({vfr}, {}, ctx);
  EXPECT_EQ(outputs.size(), 1u);
  auto output = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      outputs[0]);
  EXPECT_NE(output, nullptr);
  return output;
}

TEST(EFMAudioDecodeStageTest, NodeTypeInfo_ReportsTransformWithOneInput) {
  orc::EFMAudioDecodeStage stage;
  const auto info = stage.get_node_type_info();

  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "efm_audio_decode");
  EXPECT_EQ(info.min_inputs, 1u);
  EXPECT_EQ(info.max_inputs, 1u);
  EXPECT_EQ(stage.required_input_count(), 1u);
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(EFMAudioDecodeStageTest, Descriptors_DefaultsRoundTripThroughSetGet) {
  orc::EFMAudioDecodeStage stage;
  const auto descriptors = stage.get_parameter_descriptors();

  const auto* timecodes = find_descriptor(descriptors, "no_timecodes");
  const auto* concealment =
      find_descriptor(descriptors, "no_audio_concealment");
  ASSERT_NE(timecodes, nullptr);
  ASSERT_NE(concealment, nullptr);
  EXPECT_EQ(timecodes->type, orc::ParameterType::BOOL);
  EXPECT_EQ(concealment->type, orc::ParameterType::BOOL);
  ASSERT_TRUE(timecodes->constraints.default_value.has_value());
  ASSERT_TRUE(concealment->constraints.default_value.has_value());
  EXPECT_FALSE(std::get<bool>(*timecodes->constraints.default_value));
  EXPECT_FALSE(std::get<bool>(*concealment->constraints.default_value));

  // Defaults are reflected by get_parameters().
  auto params = stage.get_parameters();
  EXPECT_FALSE(std::get<bool>(params.at("no_timecodes")));
  EXPECT_FALSE(std::get<bool>(params.at("no_audio_concealment")));

  // Round-trip non-default values.
  EXPECT_TRUE(stage.set_parameters(
      {{"no_timecodes", true}, {"no_audio_concealment", true}}));
  params = stage.get_parameters();
  EXPECT_TRUE(std::get<bool>(params.at("no_timecodes")));
  EXPECT_TRUE(std::get<bool>(params.at("no_audio_concealment")));
}

TEST(EFMAudioDecodeStageTest, Execute_ThrowsOnMissingOrNonVfrInput) {
  orc::EFMAudioDecodeStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(EFMAudioDecodeStageTest, Execute_ThrowsWhenTrackCapReached) {
  orc::EFMAudioDecodeStage stage;
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count())
      .WillByDefault(Return(orc::kMaxAudioTracks));

  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({vfr}, {}, ctx), orc::DAGExecutionError);
}

TEST(EFMAudioDecodeStageTest, Execute_AppendsFreeRunningEfmTrack) {
  orc::EFMAudioDecodeStage stage;
  auto deps = std::make_shared<StrictMock<MockEFMAudioDecodeDeps>>();
  stage.set_deps_override(deps);

  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  const orc::AudioTrackDescriptor source_desc{
      "Analogue", orc::AudioTrackOrigin::ANALOGUE, true, {44100, 1}};
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(source_desc));

  auto output = make_output(stage, vfr, 1);

  EXPECT_EQ(output->audio_track_count(), 2u);
  EXPECT_TRUE(output->has_audio());

  // Source track descriptor forwards untouched.
  const auto fwd = output->get_audio_track_descriptor(0);
  ASSERT_TRUE(fwd.has_value());
  EXPECT_EQ(fwd->name, "Analogue");
  EXPECT_EQ(fwd->origin, orc::AudioTrackOrigin::ANALOGUE);
  EXPECT_TRUE(fwd->locked);

  // Appended track is the free-running EFM digital audio track.
  const auto efm = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(efm.has_value());
  EXPECT_EQ(efm->name, "EFM digital audio");
  EXPECT_EQ(efm->origin, orc::AudioTrackOrigin::EFM);
  EXPECT_FALSE(efm->locked);
  EXPECT_EQ(efm->sample_rate.num, 44100u);
  EXPECT_EQ(efm->sample_rate.den, 1u);

  // Free-running track answers the locked accessors with 0 / {} — and none
  // of this metadata access triggers a decode (StrictMock deps).
  EXPECT_EQ(output->get_audio_sample_count(1, orc::FrameID(0)), 0u);
  EXPECT_TRUE(output->get_audio_samples(1, orc::FrameID(0)).empty());
}

TEST(EFMAudioDecodeStageTest, LockedAudioAccess_ForwardsToSourceTracks) {
  orc::EFMAudioDecodeStage stage;
  auto deps = std::make_shared<StrictMock<MockEFMAudioDecodeDeps>>();
  stage.set_deps_override(deps);

  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  const std::vector<int16_t> frame_samples{1, -1, 2, -2};
  ON_CALL(*vfr, get_audio_sample_count(0, orc::FrameID(7)))
      .WillByDefault(Return(2u));
  ON_CALL(*vfr, get_audio_samples(0, orc::FrameID(7)))
      .WillByDefault(Return(frame_samples));
  ON_CALL(*vfr, get_audio_stream_pair_count(0)).WillByDefault(Return(42u));

  auto output = make_output(stage, vfr, 1);

  EXPECT_EQ(output->get_audio_sample_count(0, orc::FrameID(7)), 2u);
  EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(7)), frame_samples);
  EXPECT_EQ(output->get_audio_stream_pair_count(0), 42u);
}

TEST(EFMAudioDecodeStageTest, StreamAccess_DecodesLazilyAndOnlyOnce) {
  orc::EFMAudioDecodeStage stage;
  auto deps = std::make_shared<StrictMock<MockEFMAudioDecodeDeps>>();
  stage.set_deps_override(deps);

  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  auto output = make_output(stage, vfr, 1);

  // No decode so far (StrictMock would have failed). First stream access
  // triggers exactly one decode; repeated access reuses the result.
  EXPECT_CALL(*deps, decode_to_cache(_, _))
      .Times(1)
      .WillOnce(Return(orc::EFMAudioDecodeResult{true, "", 100}));

  EXPECT_EQ(output->get_audio_stream_pair_count(1), 100u);
  EXPECT_EQ(output->get_audio_stream_pair_count(1), 100u);

  // Reads are clamped to the decoded stream length before hitting the cache.
  const std::vector<int16_t> tail{10, -10, 11, -11};
  EXPECT_CALL(*deps, read_cache_pairs(98, 2)).WillOnce(Return(tail));
  EXPECT_EQ(output->get_audio_stream_samples(1, 98, 50), tail);

  // Requests entirely past the end never touch the cache.
  EXPECT_TRUE(output->get_audio_stream_samples(1, 100, 4).empty());
}

TEST(EFMAudioDecodeStageTest, StreamAccess_FailedDecodeYieldsEmptyTrack) {
  orc::EFMAudioDecodeStage stage;
  auto deps = std::make_shared<StrictMock<MockEFMAudioDecodeDeps>>();
  stage.set_deps_override(deps);

  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  auto output = make_output(stage, vfr, 0);

  // A failed decode (e.g. an EFM data disc) is attempted only once and
  // leaves the appended track empty.
  EXPECT_CALL(*deps, decode_to_cache(_, _))
      .Times(1)
      .WillOnce(
          Return(orc::EFMAudioDecodeResult{false, "no EFM t-values found", 0}));

  EXPECT_EQ(output->get_audio_stream_pair_count(0), 0u);
  EXPECT_TRUE(output->get_audio_stream_samples(0, 0, 16).empty());
}

TEST(EFMAudioDecodeStageTest, Parameters_ArePassedToDecode) {
  orc::EFMAudioDecodeStage stage;
  auto deps = std::make_shared<StrictMock<MockEFMAudioDecodeDeps>>();
  stage.set_deps_override(deps);

  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(0u));

  orc::ObservationContext ctx;
  auto outputs = stage.execute(
      {vfr}, {{"no_timecodes", true}, {"no_audio_concealment", true}}, ctx);
  auto output = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      outputs[0]);
  ASSERT_NE(output, nullptr);

  EXPECT_CALL(*deps, decode_to_cache(_, _))
      .WillOnce([](const orc::VideoFrameRepresentation&,
                   const orc::EFMAudioDecodeOptions& options) {
        EXPECT_TRUE(options.no_timecodes);
        EXPECT_TRUE(options.no_audio_concealment);
        return orc::EFMAudioDecodeResult{true, "", 1};
      });
  EXPECT_EQ(output->get_audio_stream_pair_count(0), 1u);
}

}  // namespace
}  // namespace orc_unit_test
