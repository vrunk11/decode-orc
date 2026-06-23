/*
 * File:        source_align_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for SourceAlignStage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/source_align/source_align_stage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include "../../../../orc/core/include/observation_context.h"
#include "../../include/video_frame_representation_artifact_mock.h"

using ::testing::NiceMock;
using ::testing::Return;

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

TEST(SourceAlignStageTest, RequiredInputCount_IsOne) {
  orc::SourceAlignStage stage;
  EXPECT_EQ(stage.required_input_count(), 1u);
}

TEST(SourceAlignStageTest, OutputCount_IsUnboundedSentinel) {
  orc::SourceAlignStage stage;
  EXPECT_EQ(stage.output_count(), static_cast<size_t>(UINT32_MAX));
}

TEST(SourceAlignStageTest, NodeTypeInfo_HasExpectedMetadata) {
  orc::SourceAlignStage stage;
  auto info = stage.get_node_type_info();
  EXPECT_EQ(info.type, orc::NodeType::COMPLEX);
  EXPECT_EQ(info.stage_name, "source_align");
  EXPECT_EQ(info.compatible_formats, orc::VideoFormatCompatibility::ALL);
}

TEST(SourceAlignStageTest, Descriptor_DefaultsMatchRuntimeDefaults) {
  orc::SourceAlignStage stage;
  const auto descriptors = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* alignment_map = find_descriptor(descriptors, "alignmentMap");
  ASSERT_NE(alignment_map, nullptr);
  ASSERT_TRUE(alignment_map->constraints.default_value.has_value());
  EXPECT_EQ(std::get<std::string>(*alignment_map->constraints.default_value),
            std::get<std::string>(params.at("alignmentMap")));
}

TEST(SourceAlignStageTest, SetParameters_AcceptsAlignmentMapString) {
  orc::SourceAlignStage stage;
  EXPECT_TRUE(stage.set_parameters({{"alignmentMap", std::string("1+2,2+4")}}));
  const auto params = stage.get_parameters();
  EXPECT_EQ(std::get<std::string>(params.at("alignmentMap")), "1+2,2+4");
}

TEST(SourceAlignStageTest, SetParameters_RejectsUnknownParameter) {
  orc::SourceAlignStage stage;
  EXPECT_FALSE(stage.set_parameters({{"unknown", true}}));
}

TEST(SourceAlignStageTest, Execute_ThrowsWhenNoInputsProvided) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(SourceAlignStageTest, Execute_ThrowsWhenTooManyInputsProvided) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  std::vector<orc::ArtifactPtr> inputs(17);
  EXPECT_THROW(stage.execute(inputs, {}, ctx), orc::DAGExecutionError);
}

TEST(SourceAlignStageTest, Execute_ThrowsWhenInputIsWrongType) {
  struct FakeArt : public orc::Artifact {
    FakeArt() : Artifact(orc::ArtifactID("x"), orc::Provenance{}) {}
    std::string type_name() const override { return "x"; }
  };
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({std::make_shared<FakeArt>()}, {}, ctx),
               orc::DAGExecutionError);
}

TEST(SourceAlignStageTest, Execute_ThrowsWhenManualAlignmentMapIsInvalid) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  auto source =
      std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();

  ON_CALL(*source, type_name()).WillByDefault(Return("test_vfr_artifact"));

  EXPECT_THROW(
      stage.execute({source}, {{"alignmentMap", std::string("invalid")}}, ctx),
      orc::DAGExecutionError);
}

TEST(SourceAlignStageTest, Execute_PassesThroughSingleSourceWithZeroOffset) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  auto source =
      std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();

  ON_CALL(*source, type_name()).WillByDefault(Return("test_vfr_artifact"));
  ON_CALL(*source, frame_range())
      .WillByDefault(Return(orc::FrameIDRange{0u, 9u}));
  ON_CALL(*source, frame_count()).WillByDefault(Return(10u));

  const auto outputs = stage.execute({source}, {}, ctx);

  ASSERT_EQ(outputs.size(), 1u);
  EXPECT_EQ(outputs[0].get(), source.get());  // pass-through pointer equality
}

TEST(SourceAlignStageTest, Execute_AppliesManualOffset_InFirstCommonFrameMode) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  auto source =
      std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();

  ON_CALL(*source, type_name()).WillByDefault(Return("test_vfr_artifact"));
  ON_CALL(*source, frame_range())
      .WillByDefault(Return(orc::FrameIDRange{0u, 9u}));  // 10 frames
  ON_CALL(*source, frame_count()).WillByDefault(Return(10u));

  ASSERT_TRUE(stage.set_parameters(
      {{"alignmentMode", std::string("first_common_frame")},
       {"alignmentMap", std::string("1+2")}}));

  const auto outputs = stage.execute({source}, {}, ctx);

  ASSERT_EQ(outputs.size(), 1u);
  EXPECT_NE(outputs[0].get(), source.get());

  // Skip offset 2: output should have 10 - 2 = 8 frames.
  auto vfr =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  ASSERT_NE(vfr, nullptr);
  EXPECT_EQ(vfr->frame_count(), 8u);
}

TEST(SourceAlignStageTest,
     Execute_PrependsPaddingFrames_InPadForAlignmentMode) {
  orc::SourceAlignStage stage;
  orc::ObservationContext ctx;
  auto source =
      std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();

  ON_CALL(*source, type_name()).WillByDefault(Return("test_vfr_artifact"));
  ON_CALL(*source, frame_range())
      .WillByDefault(Return(orc::FrameIDRange{0u, 9u}));  // 10 frames
  ON_CALL(*source, frame_count()).WillByDefault(Return(10u));

  ASSERT_TRUE(
      stage.set_parameters({{"alignmentMode", std::string("pad_for_alignment")},
                            {"alignmentMap", std::string("1+3")}}));

  const auto outputs = stage.execute({source}, {}, ctx);

  ASSERT_EQ(outputs.size(), 1u);
  EXPECT_NE(outputs[0].get(), source.get());

  // Pad prefix 3: output should have 3 + 10 = 13 frames.
  auto vfr =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  ASSERT_NE(vfr, nullptr);
  EXPECT_EQ(vfr->frame_count(), 13u);

  // First 3 frames must be marked as padding.
  for (orc::FrameID fid = 0; fid < 3; ++fid) {
    EXPECT_TRUE(vfr->has_frame(fid));
    auto desc = vfr->get_frame_descriptor(fid);
    ASSERT_TRUE(desc.has_value()) << "no descriptor for padding frame " << fid;
    EXPECT_TRUE(desc->is_padding_frame) << "frame " << fid << " not padding";
  }
}

}  // namespace orc_unit_test
