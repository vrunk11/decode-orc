/*
 * File:        frame_map_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for FrameMapStage parameter defaults and validation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "../../../../orc/plugins/stages/frame_map/frame_map_stage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <orc/stage/observation_context.h>

#include <algorithm>

#include "../../mocks/mock_video_frame_representation.h"

namespace orc_unit_test {

namespace {
const orc::ParameterDescriptor* find_descriptor(
    const std::vector<orc::ParameterDescriptor>& descriptors,
    const std::string& name) {
  auto it = std::find_if(
      descriptors.begin(), descriptors.end(),
      [&](const orc::ParameterDescriptor& d) { return d.name == name; });
  return it == descriptors.end() ? nullptr : &(*it);
}
}  // namespace

// ── NodeTypeInfo / interface contracts ──────────────────────────────────────

TEST(FrameMapStageTest, RequiredInputCount_IsOne) {
  orc::FrameMapStage stage;
  EXPECT_EQ(stage.required_input_count(), 1u);
}

TEST(FrameMapStageTest, OutputCount_IsOne) {
  orc::FrameMapStage stage;
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(FrameMapStageTest, NodeTypeInfo_HasExpectedMetadata) {
  orc::FrameMapStage stage;
  auto info = stage.get_node_type_info();
  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "frame_map");
  EXPECT_EQ(info.compatible_formats, orc::VideoFormatCompatibility::ALL);
}

// ── Parameter descriptors vs runtime defaults ────────────────────────────────

TEST(FrameMapStageTest, Descriptor_RangesMatchRuntimeDefault) {
  orc::FrameMapStage stage;
  const auto descs = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* d = find_descriptor(descs, "ranges");
  ASSERT_NE(d, nullptr);
  ASSERT_TRUE(d->constraints.default_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<std::string>(*d->constraints.default_value));
  ASSERT_TRUE(std::holds_alternative<std::string>(params.at("ranges")));
  EXPECT_EQ(std::get<std::string>(*d->constraints.default_value),
            std::get<std::string>(params.at("ranges")));
}

TEST(FrameMapStageTest, Descriptor_RemoveDuplicatesMatchRuntimeDefault) {
  orc::FrameMapStage stage;
  const auto descs = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* d = find_descriptor(descs, "remove_duplicates");
  ASSERT_NE(d, nullptr);
  ASSERT_TRUE(d->constraints.default_value.has_value());
  ASSERT_TRUE(std::holds_alternative<bool>(*d->constraints.default_value));
  ASSERT_TRUE(std::holds_alternative<bool>(params.at("remove_duplicates")));
  EXPECT_EQ(std::get<bool>(*d->constraints.default_value),
            std::get<bool>(params.at("remove_duplicates")));
}

TEST(FrameMapStageTest, Descriptor_PadGapsMatchRuntimeDefault) {
  orc::FrameMapStage stage;
  const auto descs = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* d = find_descriptor(descs, "pad_gaps");
  ASSERT_NE(d, nullptr);
  ASSERT_TRUE(d->constraints.default_value.has_value());
  ASSERT_TRUE(std::holds_alternative<bool>(*d->constraints.default_value));
  EXPECT_EQ(std::get<bool>(*d->constraints.default_value),
            std::get<bool>(params.at("pad_gaps")));
}

TEST(FrameMapStageTest, Descriptor_PadStrategyMatchRuntimeDefault) {
  orc::FrameMapStage stage;
  const auto descs = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* d = find_descriptor(descs, "pad_strategy");
  ASSERT_NE(d, nullptr);
  ASSERT_TRUE(d->constraints.default_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<std::string>(*d->constraints.default_value));
  EXPECT_EQ(std::get<std::string>(*d->constraints.default_value),
            std::get<std::string>(params.at("pad_strategy")));
}

// ── set_parameters validation ─────────────────────────────────────────────

TEST(FrameMapStageTest, SetParameters_AcceptsValidRanges) {
  orc::FrameMapStage stage;
  EXPECT_TRUE(stage.set_parameters({{"ranges", std::string("0-10,20-30")}}));
  EXPECT_EQ(std::get<std::string>(stage.get_parameters().at("ranges")),
            "0-10,20-30");
}

TEST(FrameMapStageTest, SetParameters_AcceptsEmptyRanges) {
  orc::FrameMapStage stage;
  EXPECT_TRUE(stage.set_parameters({{"ranges", std::string("")}}));
}

TEST(FrameMapStageTest, SetParameters_AcceptsRemoveDuplicatesTrue) {
  orc::FrameMapStage stage;
  EXPECT_TRUE(stage.set_parameters({{"remove_duplicates", true}}));
  EXPECT_TRUE(std::get<bool>(stage.get_parameters().at("remove_duplicates")));
}

TEST(FrameMapStageTest, SetParameters_AcceptsPadGapsTrue) {
  orc::FrameMapStage stage;
  EXPECT_TRUE(stage.set_parameters({{"pad_gaps", true}}));
  EXPECT_TRUE(std::get<bool>(stage.get_parameters().at("pad_gaps")));
}

TEST(FrameMapStageTest, SetParameters_AcceptsValidPadStrategies) {
  orc::FrameMapStage stage;
  EXPECT_TRUE(stage.set_parameters({{"pad_strategy", std::string("nearest")}}));
  EXPECT_TRUE(stage.set_parameters({{"pad_strategy", std::string("black")}}));
}

TEST(FrameMapStageTest, SetParameters_RejectsInvalidPadStrategy) {
  orc::FrameMapStage stage;
  EXPECT_FALSE(stage.set_parameters({{"pad_strategy", std::string("bogus")}}));
}

// ── execute() error handling ───────────────────────────────────────────────

TEST(FrameMapStageTest, Execute_ThrowsWhenInputMissing) {
  orc::FrameMapStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(FrameMapStageTest, Execute_ThrowsWhenInputIsNotVFrameR) {
  orc::FrameMapStage stage;
  orc::ObservationContext ctx;
  struct DummyArtifact : public orc::Artifact {
    DummyArtifact()
        : orc::Artifact(orc::ArtifactID("dummy"), orc::Provenance{}) {}
    std::string type_name() const override { return "dummy"; }
  };
  orc::ArtifactPtr bad = std::make_shared<DummyArtifact>();
  EXPECT_THROW(stage.execute({bad}, {}, ctx), orc::DAGExecutionError);
}

// ── Dropout hint frame IDs ──────────────────────────────────────────────────

// Regression: hints returned for a mapped frame must carry the mapped
// representation's frame ID, not the source frame's — consumers such as
// dropout_map may rely on the frame_id field.
TEST(FrameMapStageTest, GetDropoutHints_RewritesFrameIdToMappedFrame) {
  auto source =
      std::make_shared<::testing::NiceMock<MockVideoFrameRepresentation>>();

  // Source frame 5 has one dropout run (carrying its own frame id, 5).
  std::vector<orc::DropoutRun> source_runs{{orc::FrameID{5}, 1000u, 50u, 128}};
  ON_CALL(*source, get_dropout_hints(orc::FrameID{5}))
      .WillByDefault(::testing::Return(source_runs));

  // Output frame 0 maps to source frame 5.
  const orc::FrameMappedRepresentation rep(source, {orc::FrameID{5}}, {},
                                           "test");

  const auto hints = rep.get_dropout_hints(orc::FrameID{0});
  ASSERT_EQ(hints.size(), 1u);
  EXPECT_EQ(hints[0].frame_id, orc::FrameID{0});
  EXPECT_EQ(hints[0].sample_start, 1000u);
  EXPECT_EQ(hints[0].sample_count, 50u);
}

// ── Free-running audio: time-domain remapping ───────────────────────────────

namespace {

constexpr orc::FrameID kPad{UINT64_MAX};

uint64_t ntsc_offset(uint64_t frame) {
  return orc::audio_stream_pair_offset(frame, orc::kFreeRunningAudioRate,
                                       orc::VideoSystem::NTSC);
}

// NTSC source with 10 frames and one free-running 44100 Hz track whose pair
// p carries the value (p mod 32768) on both channels — so any read is
// traceable to its source stream position. NTSC keeps the per-frame windows
// non-integer (1471/1472 pairs), exercising the audio_stream_pair_offset()
// agreement demanded by the design.
class FakeFreeRunningNtscSource : public orc::VideoFrameRepresentation,
                                  public orc::Artifact {
 public:
  FakeFreeRunningNtscSource()
      : orc::Artifact(orc::ArtifactID("fake_free_running_ntsc"),
                      orc::Provenance{}) {}

  std::string type_name() const override { return "fake_free_running_ntsc"; }

  orc::FrameIDRange frame_range() const override {
    return {orc::FrameID{0}, orc::FrameID{9}};
  }
  size_t frame_count() const override { return 10; }
  bool has_frame(orc::FrameID id) const override { return id < 10; }
  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (!has_frame(id)) return std::nullopt;
    orc::FrameDescriptor desc;
    desc.frame_id = id;
    desc.system = orc::VideoSystem::NTSC;
    return desc;
  }
  const sample_type* get_frame(orc::FrameID) const override { return nullptr; }
  std::vector<sample_type> get_frame_copy(orc::FrameID) const override {
    return {};
  }
  std::optional<orc::SourceParameters> get_video_parameters() const override {
    orc::SourceParameters params;
    params.system = orc::VideoSystem::NTSC;
    return params;
  }

  size_t audio_track_count() const override { return 1; }
  std::optional<orc::AudioTrackDescriptor> get_audio_track_descriptor(
      size_t track) const override {
    if (track != 0) return std::nullopt;
    return orc::AudioTrackDescriptor{"Analogue",
                                     orc::AudioTrackOrigin::ANALOGUE, false,
                                     orc::kFreeRunningAudioRate};
  }
  uint64_t get_audio_stream_pair_count(size_t track) const override {
    return track == 0 ? ntsc_offset(10) : 0;
  }
  std::vector<int16_t> get_audio_stream_samples(
      size_t track, uint64_t first_pair, uint32_t pair_count) const override {
    if (track != 0) return {};
    const uint64_t total = ntsc_offset(10);
    if (first_pair >= total) return {};
    const auto clamped = static_cast<uint32_t>(
        std::min<uint64_t>(pair_count, total - first_pair));
    std::vector<int16_t> samples;
    samples.reserve(static_cast<size_t>(clamped) * 2);
    for (uint32_t p = 0; p < clamped; ++p) {
      const auto value = static_cast<int16_t>((first_pair + p) % 32768);
      samples.push_back(value);
      samples.push_back(value);
    }
    return samples;
  }
};

// Reads |pair_count| pairs and returns the source pair index carried in each
// pair (both channels are identical by construction).
std::vector<int16_t> read_pair_values(const orc::VideoFrameRepresentation& rep,
                                      uint64_t first_pair,
                                      uint32_t pair_count) {
  const auto samples = rep.get_audio_stream_samples(0, first_pair, pair_count);
  std::vector<int16_t> values;
  for (size_t p = 0; p + 1 < samples.size() + 1 && p * 2 < samples.size();
       ++p) {
    values.push_back(samples[p * 2]);
  }
  return values;
}

}  // namespace

TEST(FrameMapStageTest, FreeRunning_ContiguousMappingIsExactStreamSlice) {
  auto source = std::make_shared<FakeFreeRunningNtscSource>();
  // Frames 2..5 — a single monotonic source range.
  const orc::FrameMappedRepresentation rep(
      source,
      {orc::FrameID{2}, orc::FrameID{3}, orc::FrameID{4}, orc::FrameID{5}}, {},
      "contiguous");

  // The output stream is exactly [offset(2), offset(6)).
  EXPECT_EQ(rep.get_audio_stream_pair_count(0),
            ntsc_offset(6) - ntsc_offset(2));

  // A read crossing several 1471/1472-pair window boundaries stays sample
  // continuous: output pair q carries source pair offset(2) + q.
  const uint64_t start = 1470;  // inside the first window, near its edge
  const auto values = read_pair_values(rep, start, 8);
  ASSERT_EQ(values.size(), 8u);
  for (size_t p = 0; p < values.size(); ++p) {
    EXPECT_EQ(values[p],
              static_cast<int16_t>((ntsc_offset(2) + start + p) % 32768))
        << "pair " << p;
  }
}

TEST(FrameMapStageTest, FreeRunning_GeneralMappingStitchesSourceWindows) {
  auto source = std::make_shared<FakeFreeRunningNtscSource>();
  // Reordered mapping: frame 5 then frame 2.
  const orc::FrameMappedRepresentation rep(
      source, {orc::FrameID{5}, orc::FrameID{2}}, {}, "reordered");

  const uint64_t window5 = ntsc_offset(6) - ntsc_offset(5);
  const uint64_t window2 = ntsc_offset(3) - ntsc_offset(2);
  EXPECT_EQ(rep.get_audio_stream_pair_count(0), window5 + window2);

  // Two pairs either side of the join: the first window ends with the last
  // pairs of frame 5's source window, the second starts at offset(2).
  const auto values = read_pair_values(rep, window5 - 2, 4);
  ASSERT_EQ(values.size(), 4u);
  EXPECT_EQ(values[0], static_cast<int16_t>((ntsc_offset(6) - 2) % 32768));
  EXPECT_EQ(values[1], static_cast<int16_t>((ntsc_offset(6) - 1) % 32768));
  EXPECT_EQ(values[2], static_cast<int16_t>(ntsc_offset(2) % 32768));
  EXPECT_EQ(values[3], static_cast<int16_t>((ntsc_offset(2) + 1) % 32768));
}

TEST(FrameMapStageTest, FreeRunning_PaddingWindowsAreSilence) {
  auto source = std::make_shared<FakeFreeRunningNtscSource>();
  // Frame 2 followed by one padding frame (output index 1).
  const orc::FrameMappedRepresentation rep(source, {orc::FrameID{2}, kPad}, {},
                                           "padded");

  const uint64_t window2 = ntsc_offset(3) - ntsc_offset(2);
  const uint64_t pad_window = ntsc_offset(2) - ntsc_offset(1);
  EXPECT_EQ(rep.get_audio_stream_pair_count(0), window2 + pad_window);

  const auto samples = rep.get_audio_stream_samples(0, window2, 4);
  EXPECT_EQ(samples, (std::vector<int16_t>(8, 0)));
}

TEST(FrameMapStageTest, CountFreeRunningDiscontinuities_ByMappingShape) {
  using rep = orc::FrameMappedRepresentation;
  // Contiguous: no joins break phase.
  EXPECT_EQ(rep::count_free_running_discontinuities(
                {orc::FrameID{2}, orc::FrameID{3}, orc::FrameID{4}}),
            0u);
  // Reorder, duplication gap, and padding transitions each break phase;
  // padding into padding does not (silence into silence).
  EXPECT_EQ(rep::count_free_running_discontinuities(
                {orc::FrameID{5}, orc::FrameID{2}}),
            1u);
  EXPECT_EQ(rep::count_free_running_discontinuities(
                {orc::FrameID{2}, kPad, kPad, orc::FrameID{3}}),
            2u);
}

TEST(FrameMapStageTest, Execute_EmitsObservationForDiscontinuousFreeRunning) {
  orc::FrameMapStage stage;
  auto source = std::make_shared<FakeFreeRunningNtscSource>();
  orc::ObservationContext ctx;

  // 0-based stored form: frame 5 then frame 2 — one discontinuity.
  ASSERT_TRUE(stage.set_parameters({{"ranges", std::string("5,2")}}));
  stage.execute({source}, {}, ctx);

  const auto message =
      ctx.get(orc::FieldID(0), "frame_map", "free_running_audio");
  ASSERT_TRUE(message.has_value());
  EXPECT_NE(std::get<std::string>(*message).find("track(s) 0"),
            std::string::npos);
  const auto count =
      ctx.get(orc::FieldID(0), "frame_map", "free_running_discontinuities");
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(std::get<int64_t>(*count), 1);
}

TEST(FrameMapStageTest, Execute_NoObservationForContiguousFreeRunning) {
  orc::FrameMapStage stage;
  auto source = std::make_shared<FakeFreeRunningNtscSource>();
  orc::ObservationContext ctx;

  ASSERT_TRUE(stage.set_parameters({{"ranges", std::string("2-5")}}));
  stage.execute({source}, {}, ctx);

  EXPECT_FALSE(
      ctx.get(orc::FieldID(0), "frame_map", "free_running_audio").has_value());
}

// ── Locked audio: padding-frame silence sizing ──────────────────────────────

TEST(FrameMapStageTest, LockedPaddingSilence_UsesSpecPairsPerFrame) {
  auto source =
      std::make_shared<::testing::NiceMock<MockVideoFrameRepresentation>>();
  orc::SourceParameters params;
  params.system = orc::VideoSystem::NTSC;
  ON_CALL(*source, get_video_parameters())
      .WillByDefault(::testing::Return(params));
  ON_CALL(*source, get_audio_track_descriptor(3))
      .WillByDefault(::testing::Return(orc::AudioTrackDescriptor{
          "Locked", orc::AudioTrackOrigin::UNKNOWN, true,
          orc::locked_audio_sample_rate(orc::VideoSystem::NTSC)}));

  const orc::FrameMappedRepresentation rep(source, {kPad}, {}, "pad_only");

  // SMPTE 170M NTSC locked layout: 1470 stereo pairs per frame — and the
  // remap must serve ANY locked track index, not just track 0.
  const auto silence = rep.get_audio_samples(3, orc::FrameID{0});
  EXPECT_EQ(silence.size(), 1470u * 2u);
  EXPECT_EQ(rep.get_audio_sample_count(3, orc::FrameID{0}), 0u);
}

}  // namespace orc_unit_test
