/*
 * File:        audio_track_import_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for AudioTrackImportStage (WAV validation, auto
 *              lock detection, locked and free-running serving)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/audio_track_import/audio_track_import_stage.h"

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

class MockAudioTrackImportDeps : public orc::IAudioTrackImportDeps {
 public:
  MOCK_METHOD(orc::WavProbeResult, open, (const std::string& wav_path),
              (override));
  MOCK_METHOD((std::vector<int16_t>), read_pairs,
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

// PAL source with 2 frames and one existing audio track.
std::shared_ptr<NiceMock<MockVideoFrameRepresentationArtifact>>
make_pal_source() {
  auto vfr = std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();
  orc::SourceParameters params;
  params.system = orc::VideoSystem::PAL;
  ON_CALL(*vfr, get_video_parameters()).WillByDefault(Return(params));
  ON_CALL(*vfr, frame_count()).WillByDefault(Return(2u));
  ON_CALL(*vfr, frame_range())
      .WillByDefault(
          Return(orc::FrameIDRange{orc::FrameID(0), orc::FrameID(1)}));
  ON_CALL(*vfr, has_frame(_)).WillByDefault(Return(true));
  ON_CALL(*vfr, audio_track_count()).WillByDefault(Return(1u));
  return vfr;
}

std::shared_ptr<const orc::VideoFrameRepresentation> run(
    orc::AudioTrackImportStage& stage, orc::ArtifactPtr input,
    const std::map<std::string, orc::ParameterValue>& params) {
  orc::ObservationContext ctx;
  auto outputs = stage.execute({std::move(input)}, params, ctx);
  EXPECT_EQ(outputs.size(), 1u);
  auto output = std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
      outputs[0]);
  EXPECT_NE(output, nullptr);
  return output;
}

TEST(AudioTrackImportStageTest, NodeTypeInfo_ReportsTransformWithOneInput) {
  orc::AudioTrackImportStage stage;
  const auto info = stage.get_node_type_info();

  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "audio_track_import");
  EXPECT_EQ(stage.required_input_count(), 1u);
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(AudioTrackImportStageTest, Descriptors_DefaultsRoundTripThroughSetGet) {
  orc::AudioTrackImportStage stage;
  const auto descriptors = stage.get_parameter_descriptors();

  const auto* wav_path = find_descriptor(descriptors, "wav_path");
  const auto* track_name = find_descriptor(descriptors, "track_name");
  const auto* lock_mode = find_descriptor(descriptors, "lock_mode");
  ASSERT_NE(wav_path, nullptr);
  ASSERT_NE(track_name, nullptr);
  ASSERT_NE(lock_mode, nullptr);
  EXPECT_EQ(wav_path->type, orc::ParameterType::FILE_PATH);
  EXPECT_TRUE(wav_path->constraints.required);
  EXPECT_EQ(wav_path->file_extension_hint, ".wav");
  EXPECT_EQ(std::get<std::string>(*lock_mode->constraints.default_value),
            "auto");
  EXPECT_EQ(lock_mode->constraints.allowed_strings.size(), 3u);

  EXPECT_TRUE(
      stage.set_parameters({{"wav_path", std::string("/tmp/a.wav")},
                            {"track_name", std::string("Commentary")},
                            {"lock_mode", std::string("free_running")}}));
  const auto params = stage.get_parameters();
  EXPECT_EQ(std::get<std::string>(params.at("wav_path")), "/tmp/a.wav");
  EXPECT_EQ(std::get<std::string>(params.at("track_name")), "Commentary");
  EXPECT_EQ(std::get<std::string>(params.at("lock_mode")), "free_running");

  EXPECT_FALSE(stage.set_parameters({{"lock_mode", std::string("bogus")}}));
}

TEST(AudioTrackImportStageTest, Execute_ThrowsWhenWavPathUnset) {
  orc::AudioTrackImportStage stage;
  auto vfr = make_pal_source();
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({vfr}, {}, ctx), orc::DAGExecutionError);
}

TEST(AudioTrackImportStageTest, Execute_ThrowsWhenTrackCapReached) {
  orc::AudioTrackImportStage stage;
  auto vfr = make_pal_source();
  ON_CALL(*vfr, audio_track_count())
      .WillByDefault(Return(orc::kMaxAudioTracks));

  orc::ObservationContext ctx;
  EXPECT_THROW(
      stage.execute({vfr}, {{"wav_path", std::string("/tmp/a.wav")}}, ctx),
      orc::DAGExecutionError);
}

TEST(AudioTrackImportStageTest, Execute_ThrowsWhenWavInvalid) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{false, "not stereo", 0, 0}));

  auto vfr = make_pal_source();
  orc::ObservationContext ctx;
  EXPECT_THROW(
      stage.execute({vfr}, {{"wav_path", std::string("/tmp/a.wav")}}, ctx),
      orc::DAGExecutionError);
}

TEST(AudioTrackImportStageTest, Execute_ThrowsWhenFreeRunningWavNot44100) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 48000, 1000}));

  auto vfr = make_pal_source();
  orc::ObservationContext ctx;
  EXPECT_THROW(
      stage.execute({vfr}, {{"wav_path", std::string("/tmp/a.wav")}}, ctx),
      orc::DAGExecutionError);
}

TEST(AudioTrackImportStageTest, AutoMode_DetectsLockedPalWav) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  // Exactly 2 frames × 1764 pairs at the PAL locked header rate.
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 44100, 2u * 1764u}));

  auto vfr = make_pal_source();
  auto output =
      run(stage, vfr, {{"wav_path", std::string("/tmp/My Track.wav")}});

  ASSERT_EQ(output->audio_track_count(), 2u);
  const auto desc = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(desc.has_value());
  EXPECT_EQ(desc->name, "My Track");  // file stem when track_name is empty
  EXPECT_EQ(desc->origin, orc::AudioTrackOrigin::IMPORTED);
  EXPECT_TRUE(desc->locked);
  EXPECT_EQ(desc->sample_rate.num, 44100u);
  EXPECT_EQ(desc->sample_rate.den, 1u);

  // Locked serving: frame 1 reads its exact WAV slice, silence-filled past
  // any short read; stream accessors stay empty.
  EXPECT_EQ(output->get_audio_sample_count(1, orc::FrameID(1)), 1764u);
  EXPECT_CALL(*deps, read_pairs(1764, 1764))
      .WillOnce(Return(std::vector<int16_t>{7, 8, 9, 10}));
  const auto samples = output->get_audio_samples(1, orc::FrameID(1));
  ASSERT_EQ(samples.size(), 1764u * 2u);
  EXPECT_EQ(samples[0], 7);
  EXPECT_EQ(samples[3], 10);
  EXPECT_EQ(samples[4], 0);
  EXPECT_EQ(output->get_audio_stream_pair_count(1), 0u);
  EXPECT_TRUE(output->get_audio_stream_samples(1, 0, 4).empty());
}

TEST(AudioTrackImportStageTest, AutoMode_DetectsLockedNtscWavAt44056) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  // NTSC locked rate 44100000/1001 Hz rounds to a 44056 Hz WAV header.
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 44056, 2u * 1470u}));

  auto vfr = make_pal_source();
  orc::SourceParameters params;
  params.system = orc::VideoSystem::NTSC;
  ON_CALL(*vfr, get_video_parameters()).WillByDefault(Return(params));

  auto output = run(stage, vfr, {{"wav_path", std::string("/tmp/a.wav")}});

  const auto desc = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(desc.has_value());
  EXPECT_TRUE(desc->locked);
  EXPECT_EQ(desc->sample_rate.num, 44100000u);
  EXPECT_EQ(desc->sample_rate.den, 1001u);
  EXPECT_EQ(output->get_audio_sample_count(1, orc::FrameID(0)), 1470u);
}

TEST(AudioTrackImportStageTest, AutoMode_FallsBackToFreeRunning) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  // Length does not match the locked layout: free-running at 44100 Hz.
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 44100, 1000}));

  auto vfr = make_pal_source();
  auto output = run(stage, vfr,
                    {{"wav_path", std::string("/tmp/a.wav")},
                     {"track_name", std::string("Commentary")}});

  const auto desc = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(desc.has_value());
  EXPECT_EQ(desc->name, "Commentary");
  EXPECT_FALSE(desc->locked);
  EXPECT_EQ(desc->sample_rate.num, 44100u);

  EXPECT_EQ(output->get_audio_stream_pair_count(1), 1000u);
  // Reads are clamped to the WAV length; locked accessors stay empty.
  EXPECT_CALL(*deps, read_pairs(990, 10))
      .WillOnce(Return(std::vector<int16_t>(20, 3)));
  EXPECT_EQ(output->get_audio_stream_samples(1, 990, 50).size(), 20u);
  EXPECT_TRUE(output->get_audio_stream_samples(1, 1000, 4).empty());
  EXPECT_EQ(output->get_audio_sample_count(1, orc::FrameID(0)), 0u);
  EXPECT_TRUE(output->get_audio_samples(1, orc::FrameID(0)).empty());
}

TEST(AudioTrackImportStageTest, LockedMode_ForcesLockedServing) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  // Length does NOT match the locked layout, but the user declares locked.
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 44100, 1000}));

  auto vfr = make_pal_source();
  auto output = run(stage, vfr,
                    {{"wav_path", std::string("/tmp/a.wav")},
                     {"lock_mode", std::string("locked")}});

  const auto desc = output->get_audio_track_descriptor(1);
  ASSERT_TRUE(desc.has_value());
  EXPECT_TRUE(desc->locked);
  EXPECT_EQ(output->get_audio_sample_count(1, orc::FrameID(0)), 1764u);
  EXPECT_EQ(output->get_audio_stream_pair_count(1), 0u);
}

TEST(AudioTrackImportStageTest, SourceTracks_ForwardUntouched) {
  orc::AudioTrackImportStage stage;
  auto deps = std::make_shared<NiceMock<MockAudioTrackImportDeps>>();
  stage.set_deps_override(deps);
  ON_CALL(*deps, open(_))
      .WillByDefault(Return(orc::WavProbeResult{true, "", 44100, 1000}));

  auto vfr = make_pal_source();
  const orc::AudioTrackDescriptor source_desc{
      "Analogue", orc::AudioTrackOrigin::ANALOGUE, true,
      orc::AudioSampleRate{44100, 1}};
  ON_CALL(*vfr, get_audio_track_descriptor(0))
      .WillByDefault(Return(source_desc));
  ON_CALL(*vfr, get_audio_samples(0, orc::FrameID(0)))
      .WillByDefault(Return(std::vector<int16_t>{1, 2}));

  auto output = run(stage, vfr, {{"wav_path", std::string("/tmp/a.wav")}});

  const auto fwd = output->get_audio_track_descriptor(0);
  ASSERT_TRUE(fwd.has_value());
  EXPECT_EQ(fwd->name, "Analogue");
  EXPECT_EQ(output->get_audio_samples(0, orc::FrameID(0)),
            (std::vector<int16_t>{1, 2}));
}

}  // namespace
}  // namespace orc_unit_test
