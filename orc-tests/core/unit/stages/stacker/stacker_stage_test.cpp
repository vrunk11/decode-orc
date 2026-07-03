/*
 * File:        stacker_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for StackerStage defaults and parameter validation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/stacker/stacker_stage.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "../../mocks/mock_video_frame_representation.h"

using ::testing::_;
using ::testing::Return;

namespace orc_unit_test {

namespace {
const orc::ParameterDescriptor* find_descriptor(
    const std::vector<orc::ParameterDescriptor>& descriptors,
    const std::string& name) {
  auto it = std::find_if(descriptors.begin(), descriptors.end(),
                         [&](const orc::ParameterDescriptor& descriptor) {
                           return descriptor.name == name;
                         });

  return it == descriptors.end() ? nullptr : &(*it);
}
}  // namespace

TEST(StackerStageTest, RequiredInputCount_IsOne) {
  orc::StackerStage stage;
  EXPECT_EQ(stage.required_input_count(), 1u);
}

TEST(StackerStageTest, OutputCount_IsOne) {
  orc::StackerStage stage;
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(StackerStageTest, NodeTypeInfo_HasExpectedMetadata) {
  orc::StackerStage stage;
  auto info = stage.get_node_type_info();

  EXPECT_EQ(info.type, orc::NodeType::MERGER);
  EXPECT_EQ(info.stage_name, "stacker");
  EXPECT_EQ(info.compatible_formats, orc::VideoFormatCompatibility::ALL);
}

TEST(StackerStageTest, Descriptor_DefaultsMatchRuntimeDefaults) {
  orc::StackerStage stage;
  const auto descriptors = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* mode = find_descriptor(descriptors, "mode");
  const auto* threshold = find_descriptor(descriptors, "smart_threshold");
  const auto* no_diff_dod = find_descriptor(descriptors, "no_diff_dod");
  const auto* passthrough = find_descriptor(descriptors, "passthrough");
  const auto* audio_stacking = find_descriptor(descriptors, "audio_stacking");
  const auto* efm_stacking = find_descriptor(descriptors, "efm_stacking");

  ASSERT_NE(mode, nullptr);
  ASSERT_NE(threshold, nullptr);
  ASSERT_NE(no_diff_dod, nullptr);
  ASSERT_NE(passthrough, nullptr);
  ASSERT_NE(audio_stacking, nullptr);
  ASSERT_NE(efm_stacking, nullptr);

  if (!mode->constraints.default_value.has_value() ||
      !threshold->constraints.default_value.has_value() ||
      !no_diff_dod->constraints.default_value.has_value() ||
      !passthrough->constraints.default_value.has_value() ||
      !audio_stacking->constraints.default_value.has_value() ||
      !efm_stacking->constraints.default_value.has_value()) {
    FAIL() << "Expected all descriptors to have default values";
    return;
  }

  EXPECT_EQ(std::get<std::string>(*mode->constraints.default_value),
            std::get<std::string>(params.at("mode")));
  EXPECT_EQ(std::get<int32_t>(*threshold->constraints.default_value),
            std::get<int32_t>(params.at("smart_threshold")));
  EXPECT_EQ(std::get<bool>(*no_diff_dod->constraints.default_value),
            std::get<bool>(params.at("no_diff_dod")));
  EXPECT_EQ(std::get<bool>(*passthrough->constraints.default_value),
            std::get<bool>(params.at("passthrough")));
  EXPECT_EQ(std::get<std::string>(*audio_stacking->constraints.default_value),
            std::get<std::string>(params.at("audio_stacking")));
  EXPECT_EQ(std::get<std::string>(*efm_stacking->constraints.default_value),
            std::get<std::string>(params.at("efm_stacking")));
}

TEST(StackerStageTest, SetParameters_AcceptsValidStringValues) {
  orc::StackerStage stage;

  const bool result =
      stage.set_parameters({{"mode", std::string("Smart Mean")},
                            {"smart_threshold", static_cast<int32_t>(17)},
                            {"no_diff_dod", true},
                            {"passthrough", true},
                            {"audio_stacking", std::string("Median")},
                            {"efm_stacking", std::string("Disabled")}});
  const auto params = stage.get_parameters();

  EXPECT_TRUE(result);
  EXPECT_EQ(std::get<std::string>(params.at("mode")), "Smart Mean");
  EXPECT_EQ(std::get<int32_t>(params.at("smart_threshold")), 17);
  EXPECT_TRUE(std::get<bool>(params.at("no_diff_dod")));
  EXPECT_TRUE(std::get<bool>(params.at("passthrough")));
  EXPECT_EQ(std::get<std::string>(params.at("audio_stacking")), "Median");
  EXPECT_EQ(std::get<std::string>(params.at("efm_stacking")), "Disabled");
}

TEST(StackerStageTest, SetParameters_AcceptsLegacyIntegerMode) {
  orc::StackerStage stage;

  ASSERT_TRUE(stage.set_parameters({{"mode", int32_t(2)}}));

  EXPECT_EQ(std::get<std::string>(stage.get_parameters().at("mode")),
            "Smart Mean");
}

TEST(StackerStageTest, SetParameters_RejectsInvalidMode) {
  orc::StackerStage stage;
  EXPECT_FALSE(stage.set_parameters({{"mode", std::string("Nope")}}));
}

TEST(StackerStageTest, SetParameters_RejectsThresholdOutsideBounds) {
  orc::StackerStage stage;
  EXPECT_FALSE(stage.set_parameters({{"smart_threshold", int32_t(129)}}));
}

TEST(StackerStageTest, Process_ReturnsNullWhenSourcesEmpty) {
  orc::StackerStage stage;
  EXPECT_EQ(stage.process({}), nullptr);
}

TEST(StackerStageTest, Process_ReturnsOnlySourceInPassthroughMode) {
  orc::StackerStage stage;
  auto source = std::make_shared<MockVideoFrameRepresentation>();

  EXPECT_CALL(*source, has_separate_channels()).WillRepeatedly(Return(false));

  std::vector<std::shared_ptr<const orc::VideoFrameRepresentation>> sources = {
      source};

  const auto result = stage.process(sources);

  EXPECT_EQ(result.get(), source.get());
}

TEST(StackerStageTest, Process_ReturnsWrappedOutputForMultipleSources) {
  orc::StackerStage stage;
  auto src0 = std::make_shared<MockVideoFrameRepresentation>();
  auto src1 = std::make_shared<MockVideoFrameRepresentation>();

  EXPECT_CALL(*src0, has_separate_channels()).WillRepeatedly(Return(false));
  EXPECT_CALL(*src1, has_separate_channels()).WillRepeatedly(Return(false));

  std::vector<std::shared_ptr<const orc::VideoFrameRepresentation>> sources = {
      src0, src1};

  const auto result = stage.process(sources);

  ASSERT_NE(result, nullptr);
  EXPECT_NE(result.get(), src0.get());
  EXPECT_NE(result.get(), src1.get());
}

namespace {

// Minimal in-memory composite source: one frame filled with a constant value.
// NTSC geometry keeps every line at kStackWidth samples.
using sample_type = orc::VideoFrameRepresentation::sample_type;
constexpr size_t kStackWidth = 16;
constexpr size_t kStackHeight = 8;

class FakeConstantSource : public orc::VideoFrameRepresentation {
 public:
  explicit FakeConstantSource(sample_type value)
      : frame_(kStackWidth * kStackHeight, value) {}

  orc::FrameIDRange frame_range() const override {
    return {orc::FrameID{0}, orc::FrameID{0}};
  }
  size_t frame_count() const override { return 1; }
  bool has_frame(orc::FrameID id) const override {
    return id == orc::FrameID{0};
  }

  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (!has_frame(id)) return std::nullopt;
    orc::FrameDescriptor desc;
    desc.frame_id = id;
    desc.system = orc::VideoSystem::NTSC;
    desc.height = kStackHeight;
    desc.samples_total = frame_.size();
    desc.samples_per_line_nominal = kStackWidth;
    return desc;
  }

  const sample_type* get_frame(orc::FrameID id) const override {
    return has_frame(id) ? frame_.data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(orc::FrameID id) const override {
    return has_frame(id) ? frame_ : std::vector<sample_type>{};
  }

  std::optional<orc::SourceParameters> get_video_parameters() const override {
    orc::SourceParameters params;
    params.system = orc::VideoSystem::NTSC;
    params.frame_width_nominal = static_cast<int32_t>(kStackWidth);
    params.frame_height = static_cast<int32_t>(kStackHeight);
    params.black_level = 282;
    params.number_of_sequential_frames = 1;
    return params;
  }

 private:
  std::vector<sample_type> frame_;
};

}  // namespace

// Regression: get_line_samples()/get_line() on the stacked representation
// must return the stacked output, never the first source's raw samples
// (observers and analysis sinks read lines through these accessors).
TEST(StackerStageTest, LineReads_ReturnStackedOutputNotFirstSource) {
  orc::StackerStage stage;
  ASSERT_TRUE(stage.set_parameters({{"mode", std::string("Mean")}}));

  auto src0 = std::make_shared<FakeConstantSource>(100);
  auto src1 = std::make_shared<FakeConstantSource>(200);

  const auto stacked = stage.process({src0, src1});
  ASSERT_NE(stacked, nullptr);

  const sample_type* frame = stacked->get_frame(orc::FrameID{0});
  ASSERT_NE(frame, nullptr);

  for (size_t line = 0; line < kStackHeight; ++line) {
    const auto samples = stacked->get_line_samples(orc::FrameID{0}, line);
    ASSERT_EQ(samples.size(), kStackWidth) << "line " << line;
    for (size_t i = 0; i < kStackWidth; ++i) {
      // Mean of 100 and 200 — and definitely not the first source's value.
      EXPECT_EQ(samples[i], 150) << "line " << line << " sample " << i;
      EXPECT_EQ(samples[i], frame[line * kStackWidth + i]);
    }
  }
}

}  // namespace orc_unit_test
