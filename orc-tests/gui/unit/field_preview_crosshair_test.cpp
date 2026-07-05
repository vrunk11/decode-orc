/*
 * File:        field_preview_crosshair_test.cpp
 * Module:      orc-tests/gui/unit
 * Purpose:     Cross-hair scaling/resize regression tests for the preview
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <orc/stage/orc_rendering.h>

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <cmath>
#include <memory>

#include "fieldpreviewwidget.h"

namespace gui_unit_test {

namespace {

QApplication& ensureApplication() {
  if (auto* existing_app =
          qobject_cast<QApplication*>(QCoreApplication::instance())) {
    return *existing_app;
  }

  static int argc = 3;
  static char app_name[] = "orc-gui-widget-test";
  static char platform_opt[] = "-platform";
  static char platform_val[] = "offscreen";
  static char* argv[] = {app_name, platform_opt, platform_val, nullptr};
  static QApplication* app = [] {
    auto* created_app = new QApplication(argc, argv);
    created_app->setQuitOnLastWindowClosed(false);
    return created_app;
  }();
  return *app;
}

orc::PreviewImage makeGrayImage(uint32_t width = 100, uint32_t height = 50) {
  orc::PreviewImage image;
  image.width = width;
  image.height = height;
  image.rgb_data.assign(static_cast<size_t>(width) * height * 3, 128);
  return image;
}

bool isCrosshairGreen(const QColor& color) {
  return color.green() > 200 && color.red() < 100 && color.blue() < 100;
}

/// X position of the vertical cross-hair line found by scanning one row of
/// the rendered widget, or -1 when absent.
int findVerticalCrosshairX(FieldPreviewWidget& widget, int row) {
  QImage rendered(widget.size(), QImage::Format_RGB32);
  widget.render(&rendered);
  for (int x = 0; x < rendered.width(); ++x) {
    if (isCrosshairGreen(rendered.pixelColor(x, row))) {
      return x;
    }
  }
  return -1;
}

/**
 * Expected widget-space X of the cross-hair for a 100x50 image letterboxed
 * (fit) into the given widget size at square aspect: the vertical line runs
 * through the center of image pixel column `image_x`.
 */
double expectedCrosshairX(const QSize& widget_size, int image_x) {
  const double zoom =
      std::min(widget_size.width() / 100.0, widget_size.height() / 50.0);
  const double display_width = 100.0 * zoom;
  const double x_offset = (widget_size.width() - display_width) / 2.0;
  return x_offset + (image_x + 0.5) * zoom;
}

class FieldPreviewCrosshairTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ensureApplication();
    // Created after the QApplication exists (fixture members would be
    // constructed before SetUp runs).
    widget_ = std::make_unique<FieldPreviewWidget>();
    widget_->setAspectCorrection(1.0);
    widget_->setImage(makeGrayImage());
    widget_->setCrosshairsEnabled(true);
    widget_->show();
    QCoreApplication::processEvents();
  }

  void resizeAndSettle(const QSize& size) {
    widget_->resize(size);
    QCoreApplication::processEvents();
  }

  std::unique_ptr<FieldPreviewWidget> widget_;
};

TEST_F(FieldPreviewCrosshairTest, LockedCrosshairDrawnAtImagePosition) {
  resizeAndSettle(QSize(320, 240));
  widget_->updateCrosshairsPosition(50, 25);

  // Scan a row inside the image area but away from the horizontal line.
  const int found_x = findVerticalCrosshairX(*widget_, 60);
  ASSERT_GE(found_x, 0) << "No vertical cross-hair line rendered";
  EXPECT_NEAR(found_x, expectedCrosshairX(QSize(320, 240), 50), 2.0);
}

TEST_F(FieldPreviewCrosshairTest, CrosshairFollowsWidgetResize) {
  resizeAndSettle(QSize(320, 240));
  widget_->updateCrosshairsPosition(50, 25);

  const int x_before = findVerticalCrosshairX(*widget_, 60);
  ASSERT_GE(x_before, 0);

  // Resize the preview: the locked cross-hair must be remapped to the new
  // scale/letterbox, not stay at its old widget position.
  resizeAndSettle(QSize(640, 240));

  const int x_after = findVerticalCrosshairX(*widget_, 60);
  ASSERT_GE(x_after, 0) << "Cross-hair lost after resize";
  EXPECT_NEAR(x_after, expectedCrosshairX(QSize(640, 240), 50), 2.0);
  EXPECT_GT(std::abs(x_after - x_before), 10)
      << "Cross-hair did not move with the rescaled image";
}

TEST_F(FieldPreviewCrosshairTest, CrosshairFollowsAspectCorrectionChange) {
  resizeAndSettle(QSize(320, 240));
  widget_->updateCrosshairsPosition(50, 25);
  const int x_square = findVerticalCrosshairX(*widget_, 60);
  ASSERT_GE(x_square, 0);

  // Narrower display (DAR-style width correction) must pull the cross-hair
  // towards the new pixel-column position.
  widget_->setAspectCorrection(0.7);
  QCoreApplication::processEvents();

  // Corrected width 70 letterboxed into 320x240: zoom = min(320/70, 240/50).
  const double zoom = std::min(320.0 / (100.0 * 0.7), 240.0 / 50.0);
  const double display_width = 100.0 * 0.7 * zoom;
  const double x_offset = (320.0 - display_width) / 2.0;
  const double expected = x_offset + (50 + 0.5) * 0.7 * zoom;

  const int x_dar = findVerticalCrosshairX(*widget_, 60);
  ASSERT_GE(x_dar, 0);
  EXPECT_NEAR(x_dar, expected, 2.0);
}

TEST_F(FieldPreviewCrosshairTest, DisablingCrosshairsClearsLock) {
  resizeAndSettle(QSize(320, 240));
  widget_->updateCrosshairsPosition(50, 25);
  ASSERT_GE(findVerticalCrosshairX(*widget_, 60), 0);

  widget_->setCrosshairsEnabled(false);
  EXPECT_LT(findVerticalCrosshairX(*widget_, 60), 0);

  // Re-enabling does not resurrect the old lock.
  widget_->setCrosshairsEnabled(true);
  EXPECT_LT(findVerticalCrosshairX(*widget_, 60), 0);
}

}  // namespace
}  // namespace gui_unit_test
