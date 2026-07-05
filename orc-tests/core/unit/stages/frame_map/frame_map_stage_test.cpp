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

}  // namespace orc_unit_test
