/*
 * File:        vectorscope_geometry_test.cpp
 * Module:      orc-tests/gui/unit
 * Purpose:     Regression tests for shared vectorscope geometry helpers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "preview/vectorscope_geometry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace gui_unit_test {

TEST(VectorscopeGeometryTest, PlotGeometry_MatchesVectorscopeRasterMapping) {
  const orc::gui::VectorscopePlotGeometry geometry;

  EXPECT_EQ(geometry.canvas_size, 1024);
  EXPECT_EQ(geometry.plot_padding, orc::gui::kVectorscopePlotPadding);
  EXPECT_DOUBLE_EQ(geometry.centre_point.x(), 512.0);
  EXPECT_DOUBLE_EQ(geometry.centre_point.y(), 512.0);
  EXPECT_DOUBLE_EQ(geometry.plot_area.left(), 16.0);
  EXPECT_DOUBLE_EQ(geometry.plot_area.top(), 16.0);
  EXPECT_DOUBLE_EQ(geometry.plot_area.right(), 1008.0);
  EXPECT_DOUBLE_EQ(geometry.plot_area.bottom(), 1008.0);

  const QPointF origin = geometry.mapUV(0.0, 0.0);
  EXPECT_DOUBLE_EQ(origin.x(), 512.0);
  EXPECT_DOUBLE_EQ(origin.y(), 512.0);

  const QPointF top_left =
      geometry.mapUV(-orc::gui::kVectorscopeSignedFullScale,
                     orc::gui::kVectorscopeSignedFullScale);
  EXPECT_DOUBLE_EQ(top_left.x(), 16.0);
  EXPECT_DOUBLE_EQ(top_left.y(), 16.0);

  const QPointF bottom_right =
      geometry.mapUV(orc::gui::kVectorscopeSignedFullScale,
                     -orc::gui::kVectorscopeSignedFullScale);
  EXPECT_DOUBLE_EQ(bottom_right.x(), 1008.0);
  EXPECT_DOUBLE_EQ(bottom_right.y(), 1008.0);
}

TEST(VectorscopeGeometryTest, Ntsc_TargetsMagnitudeIs_925_Of_Pal) {
  // SMPTE 170M-2004 §10 / Annex A.2: the NTSC encoding equation applies 0.925
  // to all chroma: N = 0.925Y + 7.5 + 0.925(Q)sin(…) + 0.925(I)cos(…).
  // A comb decoder that does not compensate this factor outputs chroma at
  // 0.925× the GBR-input amplitude, so NTSC targets must lie at 0.925× the
  // PAL (no-scale) positions for every colour bar.
  constexpr double kIreRange = 50000.0;
  constexpr double kSetupFraction = 42.0 / 560.0;  // = 1 − 0.925

  for (int rgb = 1; rgb <= 6;
       ++rgb) {  // all six standard primaries/secondaries
    const orc::UVSample pal = orc::gui::vectorscopeTargetUv(
        rgb, 0.75, kIreRange, orc::VideoSystem::PAL);
    const orc::UVSample ntsc = orc::gui::vectorscopeTargetUv(
        rgb, 0.75, kIreRange, orc::VideoSystem::NTSC);

    EXPECT_NEAR(ntsc.u, pal.u * (1.0 - kSetupFraction), 1e-9) << "rgb=" << rgb;
    EXPECT_NEAR(ntsc.v, pal.v * (1.0 - kSetupFraction), 1e-9) << "rgb=" << rgb;
  }
}

TEST(VectorscopeGeometryTest, Ntsc_DisplayTargetsEqualRawTargets) {
  // calibrateVectorscopeDisplayUv is identity for NTSC: the comb decoder
  // already converts I/Q → U/V, so no coordinate-space rotation is required.
  // Display targets must equal the raw setup-adjusted targets without further
  // modification.
  constexpr double kIreRange = 50000.0;

  const orc::UVSample raw_target =
      orc::gui::vectorscopeTargetUv(4, 0.75, kIreRange, orc::VideoSystem::NTSC);
  const orc::UVSample display_target = orc::gui::vectorscopeDisplayTargetUv(
      4, 0.75, kIreRange, orc::VideoSystem::NTSC);

  EXPECT_DOUBLE_EQ(display_target.u, raw_target.u);
  EXPECT_DOUBLE_EQ(display_target.v, raw_target.v);
}

TEST(VectorscopeGeometryTest, Pal_DisplayTargetsRemainUnchanged) {
  constexpr double kIreRange = 50000.0;

  const orc::UVSample raw_target =
      orc::gui::vectorscopeTargetUv(4, 0.75, kIreRange, orc::VideoSystem::PAL);
  const orc::UVSample display_target = orc::gui::vectorscopeDisplayTargetUv(
      4, 0.75, kIreRange, orc::VideoSystem::PAL);

  EXPECT_DOUBLE_EQ(display_target.u, raw_target.u);
  EXPECT_DOUBLE_EQ(display_target.v, raw_target.v);
}

TEST(VectorscopeGeometryTest,
     Seventy_FivePercentTargetScalesSampleSpaceMagnitude) {
  constexpr double kIreRange = 65535.0;

  const orc::UVSample full_target =
      orc::gui::vectorscopeTargetUv(6, 1.0, kIreRange, orc::VideoSystem::PAL);
  const orc::UVSample partial_target =
      orc::gui::vectorscopeTargetUv(6, 0.75, kIreRange, orc::VideoSystem::PAL);

  const double full_magnitude = std::hypot(full_target.u, full_target.v);
  const double partial_magnitude =
      std::hypot(partial_target.u, partial_target.v);

  EXPECT_NEAR(partial_magnitude, full_magnitude * 0.75, 1e-9);
}

TEST(VectorscopeGeometryTest, StandardDegrees_MapToExpectedScreenQuadrants) {
  const orc::gui::VectorscopePlotGeometry geometry;

  const QPointF right = geometry.pointFromStandardDegrees(
      0.0, orc::gui::kVectorscopeSignedFullScale);
  const QPointF up = geometry.pointFromStandardDegrees(
      90.0, orc::gui::kVectorscopeSignedFullScale);
  const QPointF left = geometry.pointFromStandardDegrees(
      180.0, orc::gui::kVectorscopeSignedFullScale);
  const QPointF down = geometry.pointFromStandardDegrees(
      270.0, orc::gui::kVectorscopeSignedFullScale);

  EXPECT_GT(right.x(), geometry.centre_point.x());
  EXPECT_NEAR(right.y(), geometry.centre_point.y(), 1e-6);

  EXPECT_LT(up.y(), geometry.centre_point.y());
  EXPECT_NEAR(up.x(), geometry.centre_point.x(), 1e-6);

  EXPECT_LT(left.x(), geometry.centre_point.x());
  EXPECT_NEAR(left.y(), geometry.centre_point.y(), 1e-6);

  EXPECT_GT(down.y(), geometry.centre_point.y());
  EXPECT_NEAR(down.x(), geometry.centre_point.x(), 1e-6);
}

TEST(VectorscopeGeometryTest, Ntsc_IAndQAxesAreAt123And33Degrees) {
  // SMPTE 170M-2004 §7.3: NTSC I and Q are rotated 33° from the BT.601 V and
  // U axes respectively.  In (U, V) space:
  //   Positive I maps to (-sin33°, cos33°) → atan2(cos33°, -sin33°) = 123°
  //   Positive Q maps to ( cos33°, sin33°) → atan2(sin33°,  cos33°) =  33°
  // These are the angles that must appear on the NTSC vectorscope graticule.
  constexpr double kDeg33 = 33.0 * M_PI / 180.0;
  const double sin33 = std::sin(kDeg33);
  const double cos33 = std::cos(kDeg33);

  // From U = -sin33°·I + cos33°·Q; V = cos33°·I + sin33°·Q (pure I: Q=0):
  const double i_u = -sin33;
  const double i_v = cos33;
  const double i_angle_deg = std::atan2(i_v, i_u) * 180.0 / M_PI;
  EXPECT_NEAR(i_angle_deg, 123.0, 0.5);

  // From U and V equations (pure Q: I=0):
  const double q_u = cos33;
  const double q_v = sin33;
  const double q_angle_deg = std::atan2(q_v, q_u) * 180.0 / M_PI;
  EXPECT_NEAR(q_angle_deg, 33.0, 0.5);
}

}  // namespace gui_unit_test