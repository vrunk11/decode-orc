/*
 * File:        comb_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit test(s) for Comb decoder core paths
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/sinks/common/decoders/comb.h"

#include <gtest/gtest.h>

#include <cmath>

namespace orc_unit_test {
namespace {

orc::SourceParameters make_ntsc_video_params() {
  orc::SourceParameters p;
  p.system = orc::VideoSystem::NTSC;
  p.frame_width_nominal = 32;
  p.active_video_start = 16;
  p.active_video_end = 24;
  p.first_active_frame_line = 0;
  p.last_active_frame_line = 6;
  return p;
}

// Helper owning wrapper for tests: holds the sample buffers and the
// non-owning SourceField that points into them.
struct OwnedField {
  std::vector<int16_t> composite_buf;
  std::vector<int16_t> luma_buf;
  std::vector<int16_t> chroma_buf;
  SourceField field;

  static OwnedField makeYC(bool is_first_field, int16_t luma_base,
                           int16_t chroma_base, int width, int height) {
    OwnedField of;
    of.field.is_yc = true;
    of.field.is_first_field = is_first_field;
    of.field.frame_phase_id = is_first_field ? 1 : 2;
    of.field.line_count = static_cast<size_t>(height);
    of.field.samples_per_line = static_cast<size_t>(width);

    of.luma_buf.reserve(static_cast<size_t>(width * height));
    of.chroma_buf.reserve(static_cast<size_t>(width * height));
    for (int line = 0; line < height; ++line) {
      for (int x = 0; x < width; ++x) {
        of.luma_buf.push_back(static_cast<int16_t>(luma_base + line * 4 + x));
        of.chroma_buf.push_back(
            static_cast<int16_t>(chroma_base + ((x % 4) * 10)));
      }
    }
    // Pointers remain valid: std::vector move preserves data address.
    of.field.luma_data = of.luma_buf.data();
    of.field.chroma_data = of.chroma_buf.data();
    return of;
  }

  static OwnedField makeComposite(bool is_first_field, int16_t base, int width,
                                  int height) {
    OwnedField of;
    of.field.is_yc = false;
    of.field.is_first_field = is_first_field;
    of.field.frame_phase_id = is_first_field ? 1 : 2;
    of.field.line_count = static_cast<size_t>(height);
    of.field.samples_per_line = static_cast<size_t>(width);

    of.composite_buf.reserve(static_cast<size_t>(width * height));
    for (int line = 0; line < height; ++line) {
      for (int x = 0; x < width; ++x) {
        of.composite_buf.push_back(static_cast<int16_t>(base + line * 8 + x));
      }
    }
    of.field.data = of.composite_buf.data();
    return of;
  }
};

}  // namespace

TEST(CombTest, Configuration_LookAroundDependsOnDimensions) {
  Comb::Configuration c2d;
  c2d.dimensions = 2;

  Comb::Configuration c3d;
  c3d.dimensions = 3;

  EXPECT_EQ(c2d.getLookBehind(), 0);
  EXPECT_EQ(c2d.getLookAhead(), 0);
  EXPECT_EQ(c3d.getLookBehind(), 1);
  EXPECT_EQ(c3d.getLookAhead(), 1);
}

TEST(CombTest, DecodeFramesYcPath_PreservesLumaInActiveRegion) {
  const auto params = make_ntsc_video_params();

  Comb::Configuration config;
  config.dimensions = 2;
  config.phaseCompensation = false;

  Comb decoder;
  decoder.updateConfiguration(params, config);

  auto first_owned = OwnedField::makeYC(true, 1000, 2000, 32, 4);
  auto second_owned = OwnedField::makeYC(false, 3000, 4000, 32, 4);

  std::vector<SourceField> fields = {first_owned.field, second_owned.field};
  std::vector<ComponentFrame> output(1);

  decoder.decodeFrames(fields, 0, 2, output);

  EXPECT_EQ(output[0].getWidth(), 32);
  EXPECT_EQ(output[0].getHeight(), 525);  // NTSC: 263*2-1

  const double* line0 = output[0].y(0);
  const double* line1 = output[0].y(1);

  EXPECT_DOUBLE_EQ(line0[16],
                   static_cast<double>(first_owned.field.luma_data[16]));
  EXPECT_DOUBLE_EQ(line0[20],
                   static_cast<double>(first_owned.field.luma_data[20]));
  EXPECT_DOUBLE_EQ(line1[16],
                   static_cast<double>(second_owned.field.luma_data[16]));
  EXPECT_DOUBLE_EQ(line1[20],
                   static_cast<double>(second_owned.field.luma_data[20]));
}

TEST(CombTest, DecodeFramesCompositePath_MatchesGoldenVector) {
  const auto params = make_ntsc_video_params();

  Comb::Configuration config;
  config.dimensions = 2;
  config.phaseCompensation = false;

  Comb decoder;
  decoder.updateConfiguration(params, config);

  auto first_owned = OwnedField::makeComposite(true, 2500, 32, 4);
  auto second_owned = OwnedField::makeComposite(false, 2600, 32, 4);

  std::vector<SourceField> fields = {first_owned.field, second_owned.field};
  std::vector<ComponentFrame> output(1);

  decoder.decodeFrames(fields, 0, 2, output);

  EXPECT_EQ(output[0].getWidth(), 32);
  EXPECT_EQ(output[0].getHeight(), 525);  // NTSC: 263*2-1

  const double* y0 = output[0].y(0);
  const double* u0 = output[0].u(0);
  const double* v0 = output[0].v(0);

  EXPECT_DOUBLE_EQ(y0[16], 2516.0);
  EXPECT_DOUBLE_EQ(y0[20], 2520.0);
  EXPECT_DOUBLE_EQ(u0[16], 0.0);
  EXPECT_DOUBLE_EQ(u0[20], 0.0);
  EXPECT_DOUBLE_EQ(v0[16], 0.0);
  EXPECT_DOUBLE_EQ(v0[20], 0.0);
  EXPECT_TRUE(std::isfinite(y0[16]));
  EXPECT_TRUE(std::isfinite(u0[16]));
  EXPECT_TRUE(std::isfinite(v0[16]));
}

TEST(CombTest, InvalidConfiguration_DoesNotAttemptDecode) {
  auto params = make_ntsc_video_params();
  params.frame_width_nominal = 8;

  Comb::Configuration config;
  config.dimensions = 2;

  Comb decoder;
  std::vector<SourceField> fields(2);
  std::vector<ComponentFrame> output(1);

  EXPECT_NO_FATAL_FAILURE({
    decoder.updateConfiguration(params, config);
    decoder.decodeFrames(fields, 0, 2, output);
  });

  EXPECT_EQ(output[0].getWidth(), -1);
  EXPECT_EQ(output[0].getHeight(), -1);
}
}  // namespace orc_unit_test
