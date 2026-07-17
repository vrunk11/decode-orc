/*
 * File:        dropout_correct_stage_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for DropoutCorrectStage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/dropout_correct/dropout_correct_stage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <orc/stage/observation/observation_context.h>

#include <algorithm>

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

TEST(DropoutCorrectStageTest, RequiredInputCount_IsOne) {
  orc::DropoutCorrectStage stage;
  EXPECT_EQ(stage.required_input_count(), 1u);
}

TEST(DropoutCorrectStageTest, OutputCount_IsOne) {
  orc::DropoutCorrectStage stage;
  EXPECT_EQ(stage.output_count(), 1u);
}

TEST(DropoutCorrectStageTest, NodeTypeInfo_HasExpectedMetadata) {
  orc::DropoutCorrectStage stage;
  auto info = stage.get_node_type_info();
  EXPECT_EQ(info.type, orc::NodeType::TRANSFORM);
  EXPECT_EQ(info.stage_name, "dropout_correct");
  EXPECT_EQ(info.compatible_formats, orc::VideoFormatCompatibility::ALL);
}

TEST(DropoutCorrectStageTest, Descriptor_DefaultsMatchRuntimeDefaults) {
  orc::DropoutCorrectStage stage;
  const auto descriptors = stage.get_parameter_descriptors();
  const auto params = stage.get_parameters();

  const auto* overcorrect =
      find_descriptor(descriptors, "overcorrect_extension");
  const auto* match_phase = find_descriptor(descriptors, "match_chroma_phase");
  const auto* highlight = find_descriptor(descriptors, "highlight_corrections");

  ASSERT_NE(overcorrect, nullptr);
  ASSERT_NE(match_phase, nullptr);
  ASSERT_NE(highlight, nullptr);

  ASSERT_TRUE(overcorrect->constraints.default_value.has_value());
  ASSERT_TRUE(match_phase->constraints.default_value.has_value());
  ASSERT_TRUE(highlight->constraints.default_value.has_value());

  EXPECT_EQ(std::get<uint32_t>(*overcorrect->constraints.default_value),
            std::get<uint32_t>(params.at("overcorrect_extension")));
  EXPECT_EQ(std::get<bool>(*match_phase->constraints.default_value),
            std::get<bool>(params.at("match_chroma_phase")));
  EXPECT_EQ(std::get<bool>(*highlight->constraints.default_value),
            std::get<bool>(params.at("highlight_corrections")));
}

TEST(DropoutCorrectStageTest, SetParameters_AcceptsValidValues) {
  orc::DropoutCorrectStage stage;

  const bool result =
      stage.set_parameters({{"overcorrect_extension", static_cast<uint32_t>(8)},
                            {"match_chroma_phase", false},
                            {"highlight_corrections", true}});

  EXPECT_TRUE(result);

  const auto params = stage.get_parameters();
  EXPECT_EQ(std::get<uint32_t>(params.at("overcorrect_extension")), 8u);
  EXPECT_FALSE(std::get<bool>(params.at("match_chroma_phase")));
  EXPECT_TRUE(std::get<bool>(params.at("highlight_corrections")));
}

TEST(DropoutCorrectStageTest, SetParameters_RejectsOutOfRangeOvercorrect) {
  orc::DropoutCorrectStage stage;
  EXPECT_FALSE(stage.set_parameters(
      {{"overcorrect_extension", static_cast<uint32_t>(49)}}));
}

TEST(DropoutCorrectStageTest, SetParameters_RejectsUnknownParameter) {
  orc::DropoutCorrectStage stage;
  EXPECT_FALSE(stage.set_parameters({{"unknown", true}}));
}

TEST(DropoutCorrectStageTest, Execute_ThrowsWhenNoInputProvided) {
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({}, {}, ctx), orc::DAGExecutionError);
}

TEST(DropoutCorrectStageTest, Execute_ThrowsWhenInputIsWrongType) {
  struct FakeArt : public orc::Artifact {
    FakeArt() : Artifact(orc::ArtifactID("x"), orc::Provenance{}) {}
    std::string type_name() const override { return "x"; }
  };
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  EXPECT_THROW(stage.execute({std::make_shared<FakeArt>()}, {}, ctx),
               orc::DAGExecutionError);
}

TEST(DropoutCorrectStageTest, Execute_ReturnsVFrameRWhenInputIsValid) {
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  auto source =
      std::make_shared<NiceMock<MockVideoFrameRepresentationArtifact>>();

  ON_CALL(*source, type_name()).WillByDefault(Return("test_vfr"));
  ON_CALL(*source, frame_range())
      .WillByDefault(Return(orc::FrameIDRange{0u, 9u}));
  ON_CALL(*source, frame_count()).WillByDefault(Return(10u));

  const auto outputs = stage.execute({source}, {}, ctx);

  ASSERT_EQ(outputs.size(), 1u);
  auto vfr =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  EXPECT_NE(vfr, nullptr);
  // Output wraps the input so it must be a different object
  EXPECT_NE(outputs[0].get(), source.get());
}

TEST(DropoutCorrectStageTest, RunsToLineDropouts_SplitsAcrossLines) {
  // A run of 10 samples starting at sample 5 on line 0 (SPL=10)
  // Line 0: samples 5–9 (5 samples), line 1: samples 0–4 (5 samples)
  std::vector<orc::DropoutRun> runs = {
      {0u, 5u, 10u, 0u}  // frame_id=0, start=5, count=10
  };
  auto lds = orc::DropoutCorrectStage::runs_to_line_dropouts(
      runs, orc::VideoSystem::NTSC, 10);
  ASSERT_EQ(lds.size(), 2u);
  EXPECT_EQ(lds[0].line, 0u);
  EXPECT_EQ(lds[0].start_sample, 5u);
  EXPECT_EQ(lds[0].end_sample, 9u);
  EXPECT_EQ(lds[1].line, 1u);
  EXPECT_EQ(lds[1].start_sample, 0u);
  EXPECT_EQ(lds[1].end_sample, 4u);
}

TEST(DropoutCorrectStageTest, RunsToLineDropouts_WholeLineDropout) {
  std::vector<orc::DropoutRun> runs = {
      {0u, 0u, 910u, 0u}  // exactly one NTSC line
  };
  auto lds = orc::DropoutCorrectStage::runs_to_line_dropouts(
      runs, orc::VideoSystem::NTSC, 910);
  ASSERT_EQ(lds.size(), 1u);
  EXPECT_EQ(lds[0].line, 0u);
  EXPECT_EQ(lds[0].start_sample, 0u);
  EXPECT_EQ(lds[0].end_sample, 909u);
}

TEST(DropoutCorrectStageTest, RunsToLineDropouts_EmptyRunsReturnsEmpty) {
  auto lds = orc::DropoutCorrectStage::runs_to_line_dropouts(
      {}, orc::VideoSystem::NTSC, 910);
  EXPECT_TRUE(lds.empty());
}

// ---------------------------------------------------------------------------
// End-to-end composite correction
// ---------------------------------------------------------------------------
namespace {
class FakeCompositeSource : public orc::VideoFrameRepresentation,
                            public orc::Artifact {
 public:
  static constexpr size_t kSpl = 910;
  static constexpr size_t kHeight = 525;
  static constexpr size_t kDoLine = 100;
  static constexpr size_t kDoStart = 200;
  static constexpr size_t kDoEnd = 210;

  FakeCompositeSource()
      : orc::Artifact(orc::ArtifactID("fake"), orc::Provenance{}) {
    buffer_.assign(kSpl * kHeight, int16_t{100});
    for (size_t s = kDoStart; s <= kDoEnd; ++s) {
      buffer_[kDoLine * kSpl + s] = 999;  // marker "dropout" value
    }
  }
  std::string type_name() const override { return "fake_composite"; }
  orc::FrameIDRange frame_range() const override {
    return orc::FrameIDRange{0u, 1u};
  }
  size_t frame_count() const override { return 1; }
  bool has_frame(orc::FrameID id) const override { return id == 0; }
  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (id != 0) return std::nullopt;
    orc::FrameDescriptor d;
    d.frame_id = 0;
    d.system = orc::VideoSystem::NTSC;
    d.height = kHeight;
    d.samples_total = buffer_.size();
    d.samples_per_line_nominal = kSpl;
    return d;
  }
  const sample_type* get_frame(orc::FrameID id) const override {
    return id == 0 ? buffer_.data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(orc::FrameID id) const override {
    return id == 0 ? buffer_ : std::vector<sample_type>{};
  }
  std::vector<orc::DropoutRun> get_dropout_hints(
      orc::FrameID id) const override {
    if (id != 0) return {};
    orc::DropoutRun r;
    r.frame_id = 0;
    r.sample_start = kDoLine * kSpl + kDoStart;
    r.sample_count = static_cast<uint32_t>(kDoEnd - kDoStart + 1);
    r.severity = 100;
    return {r};
  }
  std::optional<orc::SourceParameters> get_video_parameters() const override {
    orc::SourceParameters p;
    p.system = orc::VideoSystem::NTSC;
    p.frame_width_nominal = static_cast<int32_t>(kSpl);
    p.frame_height = static_cast<int32_t>(kHeight);
    p.white_level = 800;
    p.black_level = 240;
    p.blanking_level = 240;
    p.sync_tip_level = 16;
    p.peak_level = 1019;
    p.active_video_start = 134;
    p.active_video_end = 753;
    p.first_active_frame_line = 40;
    p.last_active_frame_line = 480;
    return p;
  }

 private:
  std::vector<int16_t> buffer_;
};
}  // namespace

// Regression: find_replacement_line must select a replacement line even though
// the candidate distance always exceeds the zero-initialised default, so a
// composite dropout is replaced with neighbouring-line data rather than left
// untouched.
TEST(DropoutCorrectStageTest, CompositeCorrection_ReplacesDropoutPixels) {
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  auto source = std::make_shared<FakeCompositeSource>();

  const auto outputs = stage.execute({source}, {}, ctx);
  ASSERT_EQ(outputs.size(), 1u);
  auto corrected =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  ASSERT_NE(corrected, nullptr);

  const int16_t* line = corrected->get_line(0, FakeCompositeSource::kDoLine);
  ASSERT_NE(line, nullptr);

  // Before correction the dropout samples were 999; neighbours are 100.
  for (size_t s = FakeCompositeSource::kDoStart;
       s <= FakeCompositeSource::kDoEnd; ++s) {
    EXPECT_EQ(line[s], 100) << "sample " << s << " not corrected";
  }
}

// The whole-frame accessors (get_frame / get_frame_copy) must also return the
// corrected samples — the chroma decoder reads the frame buffer this way, so a
// correction visible only through get_line() would leave it decoding the
// uncorrected dropout as a black line.
TEST(DropoutCorrectStageTest, CompositeCorrection_VisibleThroughGetFrame) {
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  auto source = std::make_shared<FakeCompositeSource>();

  const auto outputs = stage.execute({source}, {}, ctx);
  ASSERT_EQ(outputs.size(), 1u);
  auto corrected =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  ASSERT_NE(corrected, nullptr);

  const int16_t* frame = corrected->get_frame(0);
  ASSERT_NE(frame, nullptr);
  const std::vector<int16_t> frame_copy = corrected->get_frame_copy(0);

  const size_t base = FakeCompositeSource::kDoLine * FakeCompositeSource::kSpl;
  for (size_t s = FakeCompositeSource::kDoStart;
       s <= FakeCompositeSource::kDoEnd; ++s) {
    EXPECT_EQ(frame[base + s], 100) << "get_frame sample " << s;
    EXPECT_EQ(frame_copy[base + s], 100) << "get_frame_copy sample " << s;
  }
}

// ---------------------------------------------------------------------------
// Replacement-line selection: phase (field) preference and dropout overlap
// ---------------------------------------------------------------------------
// Every line is filled with its own line index as the sample value, so the
// corrected sample value identifies which source line was copied.  Dropout
// lines have their dropout region overwritten with a sentinel so that copying a
// damaged line is detectable.
namespace {
constexpr int16_t kDropSentinel = 999;

class FakeIndexedSource : public orc::VideoFrameRepresentation,
                          public orc::Artifact {
 public:
  static constexpr size_t kSpl = 910;
  static constexpr size_t kHeight = 525;   // NTSC
  static constexpr size_t kField1 = 263;   // kNtscField1Lines
  static constexpr size_t kDoStart = 300;  // active-video region
  static constexpr size_t kDoEnd = 310;

  explicit FakeIndexedSource(std::vector<uint32_t> dropout_lines)
      : orc::Artifact(orc::ArtifactID("fake"), orc::Provenance{}),
        dropout_lines_(std::move(dropout_lines)) {
    buffer_.resize(kSpl * kHeight);
    for (size_t line = 0; line < kHeight; ++line) {
      for (size_t s = 0; s < kSpl; ++s) {
        buffer_[line * kSpl + s] = static_cast<int16_t>(line);
      }
    }
    for (uint32_t line : dropout_lines_) {
      for (size_t s = kDoStart; s <= kDoEnd; ++s) {
        buffer_[line * kSpl + s] = kDropSentinel;
      }
    }
  }
  std::string type_name() const override { return "fake_indexed"; }
  orc::FrameIDRange frame_range() const override {
    return orc::FrameIDRange{0u, 1u};
  }
  size_t frame_count() const override { return 1; }
  bool has_frame(orc::FrameID id) const override { return id == 0; }
  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (id != 0) return std::nullopt;
    orc::FrameDescriptor d;
    d.frame_id = 0;
    d.system = orc::VideoSystem::NTSC;
    d.height = kHeight;
    d.samples_total = buffer_.size();
    d.samples_per_line_nominal = kSpl;
    return d;
  }
  const sample_type* get_frame(orc::FrameID id) const override {
    return id == 0 ? buffer_.data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(orc::FrameID id) const override {
    return id == 0 ? buffer_ : std::vector<sample_type>{};
  }
  std::vector<orc::DropoutRun> get_dropout_hints(
      orc::FrameID id) const override {
    std::vector<orc::DropoutRun> runs;
    if (id != 0) return runs;
    for (uint32_t line : dropout_lines_) {
      orc::DropoutRun r;
      r.frame_id = 0;
      r.sample_start = line * kSpl + kDoStart;
      r.sample_count = static_cast<uint32_t>(kDoEnd - kDoStart + 1);
      r.severity = 100;
      runs.push_back(r);
    }
    return runs;
  }
  std::optional<orc::SourceParameters> get_video_parameters() const override {
    orc::SourceParameters p;
    p.system = orc::VideoSystem::NTSC;
    p.frame_width_nominal = static_cast<int32_t>(kSpl);
    p.frame_height = static_cast<int32_t>(kHeight);
    p.white_level = 800;
    p.black_level = 240;
    p.blanking_level = 240;
    p.sync_tip_level = 16;
    p.peak_level = 1019;
    p.active_video_start = 134;
    p.active_video_end = 753;
    p.first_active_frame_line = 40;
    p.last_active_frame_line = 480;
    return p;
  }

 private:
  std::vector<int16_t> buffer_;
  std::vector<uint32_t> dropout_lines_;
};

int16_t corrected_sample_at(const std::shared_ptr<FakeIndexedSource>& source,
                            uint32_t line) {
  orc::DropoutCorrectStage stage;
  orc::ObservationContext ctx;
  const auto outputs = stage.execute({source}, {}, ctx);
  EXPECT_EQ(outputs.size(), 1u);
  auto corrected =
      std::dynamic_pointer_cast<orc::VideoFrameRepresentation>(outputs[0]);
  EXPECT_NE(corrected, nullptr);
  const int16_t* row = corrected->get_line(0, line);
  EXPECT_NE(row, nullptr);
  return row[(FakeIndexedSource::kDoStart + FakeIndexedSource::kDoEnd) / 2];
}
}  // namespace

// A field-1 target must be repaired from another field-1 (intrafield) line so
// the colour subcarrier phase is preserved, not from the spatially-nearer
// field-2 line.
TEST(DropoutCorrectStageTest, PrefersIntrafieldReplacement) {
  auto source = std::make_shared<FakeIndexedSource>(std::vector<uint32_t>{100});
  const int16_t v = corrected_sample_at(source, 100);
  EXPECT_NE(v, kDropSentinel);
  EXPECT_LT(v, static_cast<int16_t>(FakeIndexedSource::kField1))
      << "replacement came from field 2 (line " << v << ") — wrong phase";
}

// A replacement line that itself carries a dropout over the same samples must
// be skipped in favour of a clean line.
TEST(DropoutCorrectStageTest, SkipsReplacementLinesWithOverlappingDropout) {
  // Damage the target and both nearest phase-matched candidates (NTSC step 2 →
  // 98/102); the search must skip them to the next in-phase lines (96/104).
  auto source =
      std::make_shared<FakeIndexedSource>(std::vector<uint32_t>{98, 100, 102});
  const int16_t v = corrected_sample_at(source, 100);
  EXPECT_NE(v, kDropSentinel) << "copied samples from a dropout line";
  EXPECT_TRUE(v == 96 || v == 104)
      << "expected a clean in-phase line (96/104), got " << v;
}

}  // namespace orc_unit_test
