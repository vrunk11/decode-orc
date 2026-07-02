/*
 * File:        vectorscope_geometry.h
 * Module:      orc-gui
 * Purpose:     Shared vectorscope geometry and target math helpers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ORC_GUI_PREVIEW_VECTORSCOPE_GEOMETRY_H
#define ORC_GUI_PREVIEW_VECTORSCOPE_GEOMETRY_H

#include <orc/stage/orc_vectorscope.h>

#include <QPointF>
#include <QRectF>
#include <cmath>

namespace orc::gui {

constexpr int kVectorscopeCanvasSize = 1024;
constexpr int kVectorscopePlotPadding = 16;
constexpr int kVectorscopeCircleStrokeWidth = 4;
constexpr int kVectorscopeAxisStrokeWidth = 2;
constexpr int kVectorscopeMajorMarkerStrokeWidth = 2;
constexpr int kVectorscopeMinorMarkerStrokeWidth = 1;
constexpr double kVectorscopeSignedFullScale = 32768.0;
constexpr double kVectorscopeUvRange = kVectorscopeSignedFullScale * 2.0;

inline double standardDegreesToScopeRadians(double standard_degrees) {
  // Standard vectorscope degrees are counterclockwise (0=right, 90=up).
  // This renderer uses clockwise-positive angles for screen mapping.
  return (-standard_degrees * M_PI) / 180.0;
}

struct VectorscopePlotGeometry {
  explicit VectorscopePlotGeometry(
      int canvas_size_pixels = kVectorscopeCanvasSize)
      : canvas_size(canvas_size_pixels),
        plot_padding(std::min(kVectorscopePlotPadding, canvas_size_pixels / 4)),
        plot_span_pixels(
            static_cast<double>(canvas_size_pixels - (plot_padding * 2))),
        pixels_per_uv_unit(plot_span_pixels / kVectorscopeUvRange),
        plot_area(static_cast<double>(plot_padding),
                  static_cast<double>(plot_padding), plot_span_pixels,
                  plot_span_pixels),
        centre_point(plot_area.center()) {}

  QPointF mapUV(double u, double v) const {
    return {centre_point.x() + (u * pixels_per_uv_unit),
            centre_point.y() - (v * pixels_per_uv_unit)};
  }

  QPointF pointFromVectorscopeAngle(double angle_radians,
                                    double magnitude_uv) const {
    return mapUV(std::cos(angle_radians) * magnitude_uv,
                 -std::sin(angle_radians) * magnitude_uv);
  }

  QPointF pointFromStandardDegrees(double standard_degrees,
                                   double magnitude_uv) const {
    return pointFromVectorscopeAngle(
        standardDegreesToScopeRadians(standard_degrees), magnitude_uv);
  }

  double magnitudeToPixels(double magnitude_uv) const {
    return magnitude_uv * pixels_per_uv_unit;
  }

  double pixelsToMagnitude(double magnitude_pixels) const {
    return magnitude_pixels / pixels_per_uv_unit;
  }

  int canvas_size;
  int plot_padding;
  double plot_span_pixels;
  double pixels_per_uv_unit;
  QRectF plot_area;
  QPointF centre_point;
};

// ============================================================================
// normalizedRgbToUv
// ============================================================================
// Convert normalised linear RGB (components in [0, 1]) to the U/V chrominance
// representation used for vectorscope display.
//
// Matrix derivation (ITU-R BT.470-6 §1.1.2 / EBU Tech. 3280-E §2.1):
//
//   Y  =  0.299 R + 0.587 G + 0.114 B          (BT.601 luminance equation)
//
//   U  = ku * (B - Y)     ku = 0.492111          (PAL/NTSC U modulation factor)
//   V  = kv * (R - Y)     kv = 0.877283          (PAL/NTSC V modulation factor)
//
//   Expanding (B - Y) and (R - Y) with the BT.601 Y equation:
//
//   U_R = ku * ( 0 - 0.299) = -0.147141
//   U_G = ku * ( 0 - 0.587) = -0.288869    [columns: kU * {-Y_R, -Y_G, 1-Y_B}]
//   U_B = ku * ( 1 - 0.114) =  0.436010
//
//   V_R = kv * ( 1 - 0.299) =  0.614975
//   V_G = kv * ( 0 - 0.587) = -0.514965    [columns: kV * {1-Y_R, -Y_G, -Y_B}]
//   V_B = kv * ( 0 - 0.114) = -0.100010
//
// amplitude_scale maps the U/V result to the display's signed full-scale.
inline orc::UVSample normalizedRgbToUv(double red, double green, double blue,
                                       double amplitude_scale) {
  // ITU-R BT.470-6 §1.1.2 / EBU Tech. 3280-E §2.1
  const double u = (red * -0.147141) + (green * -0.288869) + (blue * 0.436010);
  const double v = (red * 0.614975) + (green * -0.514965) + (blue * -0.100010);
  return {u * amplitude_scale, v * amplitude_scale};
}

inline orc::UVSample calibrateVectorscopeDisplayUv(
    const orc::UVSample& sample, orc::VideoSystem /*system*/) {
  // The NTSC comb decoder converts I/Q → U/V (comb.cpp transformIQ) before
  // writing to ComponentFrame, so decoded NTSC samples are already in the
  // same BT.601 U/V coordinate space as PAL.  No calibration is needed.
  return sample;
}

// ============================================================================
// vectorscopeTargetUv
// ============================================================================
// Return the U/V position for an ideal colour-bar patch at a given saturation
// percentage.  `rgb` encodes the primary/secondary colour as a 3-bit mask
// (bit2=R, bit1=G, bit0=B).
//
// GBR encoder inputs have no setup (SMPTE 170M-2004 §4.3 / ITU-R BT.470-6):
// "off" components are at 0 (blanking) and "on" components are at `percent`.
//
// NTSC / PAL_M encoding (SMPTE 170M-2004 §10 / Annex A.2):
//   N = 0.925(Y) + 7.5 + 0.925(Q)sin(…+33°) + 0.925(I)cos(…+33°)
//   The 0.925 factor is applied to all chroma in the encoding equation.
//   A comb decoder that does not compensate this factor outputs chroma at
//   0.925× the GBR-input amplitude.  Targets must therefore sit at 0.925×
//   the PAL (no-scale) positions so that a correctly decoded NTSC signal
//   lands on the crosshairs.
inline orc::UVSample vectorscopeTargetUv(int rgb, double percent,
                                         double ire_range,
                                         orc::VideoSystem system) {
  const double red = ((rgb >> 2) & 1) ? percent : 0.0;
  const double green = ((rgb >> 1) & 1) ? percent : 0.0;
  const double blue = (rgb & 1) ? percent : 0.0;
  const orc::UVSample uv = normalizedRgbToUv(red, green, blue, ire_range);

  // SMPTE 170M-2004 §10 / Annex A.2: NTSC and PAL_M apply 0.925 to chroma.
  const bool has_smpte_chroma_scale =
      (system == orc::VideoSystem::NTSC || system == orc::VideoSystem::PAL_M);
  constexpr double kSmpteChromaScale = 0.925;
  if (has_smpte_chroma_scale) {
    return {uv.u * kSmpteChromaScale, uv.v * kSmpteChromaScale};
  }
  return uv;
}

inline orc::UVSample vectorscopeDisplayTargetUv(int rgb, double percent,
                                                double ire_range,
                                                orc::VideoSystem system) {
  return calibrateVectorscopeDisplayUv(
      vectorscopeTargetUv(rgb, percent, ire_range, system), system);
}

}  // namespace orc::gui

#endif  // ORC_GUI_PREVIEW_VECTORSCOPE_GEOMETRY_H