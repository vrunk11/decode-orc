/*
 * File:        vectorscope_analysis_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for vectorscope extraction from ComponentFrame
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/core/analysis/vectorscope/vectorscope_analysis.h"

#include <gtest/gtest.h>

#include "../../../../orc/plugins/stages/sinks/common/decoders/componentframe.h"
#include "../../../../orc/view-types/orc_preview_carriers.h"

namespace orc_unit_test {
TEST(VectorscopeAnalysisTest,
     ExtractFromComponentFrame_UsesActivePictureWindow) {
  orc::SourceParameters source_parameters;
  source_parameters.system = orc::VideoSystem::NTSC;
  source_parameters.frame_width_nominal = 8;
  source_parameters.active_video_start = 2;
  source_parameters.active_video_end = 6;
  source_parameters.first_active_frame_line = 1;
  source_parameters.last_active_frame_line = 6;
  // Set CVBS levels so normalization works correctly.  Active-area test values
  // (y*100+x ≈ 102-505) stay well below the clamp threshold; the outside-area
  // sentinel (10000) saturates to ±32767 after division by level_range (600).
  source_parameters.blanking_level = 200;
  source_parameters.white_level = 800;

  ComponentFrame frame;
  frame.init(source_parameters, false);

  for (int32_t y = 0; y < frame.getHeight(); ++y) {
    double* u_line = frame.u(y);
    double* v_line = frame.v(y);

    for (int32_t x = 0; x < frame.getWidth(); ++x) {
      const bool inside_active =
          y >= source_parameters.first_active_frame_line &&
          y < source_parameters.last_active_frame_line &&
          x >= source_parameters.active_video_start &&
          x < source_parameters.active_video_end;

      if (inside_active) {
        u_line[x] = static_cast<double>((y * 100) + x);
        v_line[x] = -static_cast<double>((y * 100) + x);
      } else {
        // Sentinel value that saturates to ±32767 after normalisation
        u_line[x] = 10000.0;
        v_line[x] = -10000.0;
      }
    }
  }

  const auto data = orc::VectorscopeAnalysisTool::extractFromComponentFrame(
      frame, source_parameters, 42, 1);

  EXPECT_EQ(data.field_number, 42u);
  EXPECT_EQ(data.width, 4u);
  EXPECT_EQ(data.height, 5u);
  ASSERT_EQ(data.samples.size(), 20u);

  bool saw_first_field = false;
  bool saw_second_field = false;

  for (const auto& sample : data.samples) {
    // Active-area values (≤505) normalised by level_range (600) stay below
    // the ±32767 saturation ceiling that outside-area sentinels reach.
    EXPECT_LT(std::abs(sample.u), 32767.0);
    EXPECT_LT(std::abs(sample.v), 32767.0);

    if (sample.field_id == 0) {
      saw_first_field = true;
    }
    if (sample.field_id == 1) {
      saw_second_field = true;
    }
  }

  EXPECT_TRUE(saw_first_field);
  EXPECT_TRUE(saw_second_field);
}

TEST(VectorscopeAnalysisTest, ExtractFromColourFrameCarrier_CanUseFullFrame) {
  orc::ColourFrameCarrier carrier;
  carrier.system = orc::VideoSystem::NTSC;
  carrier.width = 6;
  carrier.height = 4;
  carrier.active_x_start = 1;
  carrier.active_x_end = 5;
  carrier.active_y_start = 1;
  carrier.active_y_end = 3;
  // uv_range = 900: active-area values (y*100+x ≤ 204) stay below the clamp
  // threshold; the sentinel (10000) saturates to ±32767.
  carrier.cvbs_blanking = 100.0;
  carrier.cvbs_white = 1000.0;

  const size_t sample_count =
      static_cast<size_t>(carrier.width) * static_cast<size_t>(carrier.height);
  carrier.y_plane.assign(sample_count, 0.0);
  carrier.u_plane.resize(sample_count);
  carrier.v_plane.resize(sample_count);

  for (uint32_t y = 0; y < carrier.height; ++y) {
    for (uint32_t x = 0; x < carrier.width; ++x) {
      const size_t index = static_cast<size_t>(y) * carrier.width + x;
      const bool inside_active =
          y >= carrier.active_y_start && y < carrier.active_y_end &&
          x >= carrier.active_x_start && x < carrier.active_x_end;

      if (inside_active) {
        carrier.u_plane[index] = static_cast<double>((y * 100) + x);
        carrier.v_plane[index] = -static_cast<double>((y * 100) + x);
      } else {
        // Sentinel value that saturates to ±32767 after normalisation
        carrier.u_plane[index] = 10000.0;
        carrier.v_plane[index] = -10000.0;
      }
    }
  }

  const auto active_data =
      orc::VectorscopeAnalysisTool::extractFromColourFrameCarrier(carrier, 12,
                                                                  1, true);
  const auto full_data =
      orc::VectorscopeAnalysisTool::extractFromColourFrameCarrier(carrier, 12,
                                                                  1, false);

  EXPECT_EQ(active_data.field_number, 12u);
  EXPECT_EQ(active_data.width, 4u);
  EXPECT_EQ(active_data.height, 2u);
  ASSERT_EQ(active_data.samples.size(), 8u);

  EXPECT_EQ(full_data.field_number, 12u);
  EXPECT_EQ(full_data.width, 6u);
  EXPECT_EQ(full_data.height, 4u);
  ASSERT_EQ(full_data.samples.size(), 24u);

  // Active-area values (≤204) normalised by uv_range (900) stay below the
  // ±32767 saturation ceiling that outside-area sentinels reach.
  for (const auto& sample : active_data.samples) {
    EXPECT_LT(std::abs(sample.u), 32767.0);
    EXPECT_LT(std::abs(sample.v), 32767.0);
  }

  bool saw_full_frame_only_sample = false;
  for (const auto& sample : full_data.samples) {
    // Outside-area sentinels (10000 / uv_range 900 > 1) saturate to ±32767.
    if (std::abs(sample.u) == 32767.0 && std::abs(sample.v) == 32767.0) {
      saw_full_frame_only_sample = true;
      break;
    }
  }

  EXPECT_TRUE(saw_full_frame_only_sample);
}
}  // namespace orc_unit_test