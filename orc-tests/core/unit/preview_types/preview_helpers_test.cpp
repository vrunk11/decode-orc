/*
 * File:        preview_helpers_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for standard preview rendering line indexing
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <orc/stage/cvbs_signal_constants.h>
#include <orc/stage/frame_line_util.h>
#include <orc/stage/preview_helpers.h>
#include <orc/stage/video_frame_representation.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace orc_unit_test {

namespace {

// PAL geometry: 625 lines of 1135 samples nominal, with lines 312 and 624
// carrying two extra samples each (EBU Tech. 3280-E §1.3.1). Every field-2
// line therefore starts 2 samples past a naive line * 1135 stride, which is
// exactly the layout property the preview renderer must respect.
constexpr size_t kPalWidth = 1135;
constexpr size_t kPalHeight = 625;

// Column of the bright marker sample written into every line.
constexpr uint32_t kMarkerX = 3;

// In-memory PAL YC representation with the correct non-uniform flat layout.
// Each luma/chroma line is black except for a white marker at kMarkerX, so a
// rendered row shows exactly where the renderer believes the line starts.
class FakePalYcRepresentation : public orc::VideoFrameRepresentation {
 public:
  FakePalYcRepresentation() {
    const size_t total = orc::frame_line_sample_offset(orc::VideoSystem::PAL,
                                                       kPalWidth, kPalHeight);
    luma_.assign(total, static_cast<sample_type>(orc::kPalBlack));
    chroma_.assign(total, static_cast<sample_type>(orc::kPalBlack));
    for (size_t line = 0; line < kPalHeight; ++line) {
      const size_t offset =
          orc::frame_line_sample_offset(orc::VideoSystem::PAL, kPalWidth, line);
      luma_[offset + kMarkerX] = static_cast<sample_type>(orc::kPalWhite);
      chroma_[offset + kMarkerX] = static_cast<sample_type>(orc::kPalWhite);
    }
  }

  orc::FrameIDRange frame_range() const override { return {0, 0}; }
  size_t frame_count() const override { return 1; }
  bool has_frame(orc::FrameID id) const override { return id == 0; }

  std::optional<orc::FrameDescriptor> get_frame_descriptor(
      orc::FrameID id) const override {
    if (id != 0) return std::nullopt;
    orc::FrameDescriptor desc;
    desc.frame_id = 0;
    desc.system = orc::VideoSystem::PAL;
    desc.height = kPalHeight;
    desc.samples_total = luma_.size();
    desc.samples_per_line_nominal = kPalWidth;
    return desc;
  }

  const sample_type* get_frame(orc::FrameID id) const override {
    return id == 0 ? luma_.data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(orc::FrameID id) const override {
    return id == 0 ? luma_ : std::vector<sample_type>{};
  }

  bool has_separate_channels() const override { return true; }
  const sample_type* get_frame_luma(orc::FrameID id) const override {
    return id == 0 ? luma_.data() : nullptr;
  }
  const sample_type* get_frame_chroma(orc::FrameID id) const override {
    return id == 0 ? chroma_.data() : nullptr;
  }
  const sample_type* get_line_luma(orc::FrameID id,
                                   size_t line) const override {
    return line_ptr(luma_, id, line);
  }
  const sample_type* get_line_chroma(orc::FrameID id,
                                     size_t line) const override {
    return line_ptr(chroma_, id, line);
  }

  std::optional<orc::SourceParameters> get_video_parameters() const override {
    orc::SourceParameters params;
    params.system = orc::VideoSystem::PAL;
    params.frame_width_nominal = static_cast<int32_t>(kPalWidth);
    params.frame_height = static_cast<int32_t>(kPalHeight);
    params.sync_tip_level = orc::kPalSyncTip;
    params.black_level = orc::kPalBlack;
    params.white_level = orc::kPalWhite;
    params.peak_level = orc::kPalPeak;
    return params;
  }

 private:
  const sample_type* line_ptr(const std::vector<sample_type>& plane,
                              orc::FrameID id, size_t line) const {
    if (id != 0 || line >= kPalHeight) return nullptr;
    return plane.data() + orc::frame_line_sample_offset(orc::VideoSystem::PAL,
                                                        kPalWidth, line);
  }

  std::vector<sample_type> luma_;
  std::vector<sample_type> chroma_;
};

uint8_t gray_at(const orc::PreviewImage& image, uint32_t row, uint32_t x) {
  return image.rgb_data[(static_cast<size_t>(row) * image.width + x) * 3];
}

// Every rendered row must show the marker at kMarkerX and darkness two
// samples later. A renderer that strides field 2 with a fixed line * 1135
// offset reads those lines 2 samples early, moving the marker to kMarkerX + 2.
void expect_marker_aligned_on_all_rows(const orc::PreviewImage& image) {
  ASSERT_TRUE(image.is_valid());
  ASSERT_EQ(image.width, static_cast<uint32_t>(kPalWidth));
  ASSERT_EQ(image.height, static_cast<uint32_t>(kPalHeight));
  for (uint32_t row = 0; row < image.height; ++row) {
    ASSERT_GT(gray_at(image, row, kMarkerX), 128) << "row " << row;
    ASSERT_LT(gray_at(image, row, kMarkerX + 2), 128) << "row " << row;
  }
}

}  // namespace

TEST(PreviewHelpersTest, SequentialLumaPreview_UsesPalLineOffsets) {
  auto representation = std::make_shared<FakePalYcRepresentation>();
  const auto image = orc::PreviewHelpers::render_standard_preview(
      representation, "sequential_clamped_y", 0);
  expect_marker_aligned_on_all_rows(image);
}

TEST(PreviewHelpersTest, SequentialChromaPreview_UsesPalLineOffsets) {
  auto representation = std::make_shared<FakePalYcRepresentation>();
  const auto image = orc::PreviewHelpers::render_standard_preview(
      representation, "sequential_clamped_c", 0);
  expect_marker_aligned_on_all_rows(image);
}

TEST(PreviewHelpersTest, InterlacedLumaPreview_UsesPalLineOffsets) {
  auto representation = std::make_shared<FakePalYcRepresentation>();
  const auto image = orc::PreviewHelpers::render_standard_preview(
      representation, "interlaced_clamped_y", 0);
  expect_marker_aligned_on_all_rows(image);
}

TEST(PreviewHelpersTest, CompositePathPreview_UsesPalLineOffsets) {
  auto representation = std::make_shared<FakePalYcRepresentation>();
  const auto image = orc::PreviewHelpers::render_standard_preview(
      representation, "sequential_clamped", 0);
  expect_marker_aligned_on_all_rows(image);
}

}  // namespace orc_unit_test
