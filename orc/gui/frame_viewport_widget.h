/*
 * File:        frame_viewport_widget.h
 * Module:      orc-gui
 * Purpose:     Reusable zoomable frame viewport widget for scroll areas
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef FRAME_VIEWPORT_WIDGET_H
#define FRAME_VIEWPORT_WIDGET_H

#include <QImage>
#include <QWidget>

#include "frame_view_geometry.h"

class QScrollArea;

/**
 * @brief Zoomable, aspect-corrected frame viewport for use inside a
 * QScrollArea
 *
 * Displays a rendered frame image with:
 * - Zoom (buttons/API plus Ctrl+wheel zoom-at-cursor)
 * - Aspect-ratio correction (width scale, matching the preview dialog)
 * - Fit-to-viewport
 * - Widget<->image coordinate mapping via orc::gui::FrameViewGeometry
 *
 * The widget resizes itself to the zoomed display size; panning is provided
 * by the enclosing QScrollArea (plain wheel events propagate to it).
 *
 * Subclasses draw interactive overlays by overriding paintOverlay(), which is
 * called after the frame image is painted. Overlays are drawn in widget
 * coordinates (crisp at any zoom) using the coordinate mapping accessors.
 *
 * Thread safety: GUI thread only.
 */
class FrameViewportWidget : public QWidget {
  Q_OBJECT

 public:
  explicit FrameViewportWidget(QWidget* parent = nullptr);
  ~FrameViewportWidget() override = default;

  /// Set the frame image to display (null image clears the display).
  void setImage(const QImage& image);
  void clearImage();
  bool hasImage() const { return !image_.isNull(); }
  QSize imageSize() const { return image_.size(); }

  /// Width scale factor for display (1.0 = SAR 1:1; ~0.7 for DAR 4:3).
  void setAspectCorrection(double correction);
  double aspectCorrection() const { return geometry_.aspectCorrection(); }

  /// Zoom control. Levels are clamped to the configured range.
  void setZoomLevel(double zoom);
  double zoomLevel() const { return geometry_.zoom(); }
  void setZoomRange(double min_zoom, double max_zoom);
  void zoomIn();
  void zoomOut();

  /// Fit the image to the enclosing scroll-area viewport (or own size).
  void fitToViewport();

  /// @name Coordinate mapping (widget <-> image space)
  /// @{
  QPointF imageFromWidget(const QPointF& widget_pos) const {
    return geometry_.imageFromWidget(widget_pos);
  }
  QPointF widgetFromImage(const QPointF& image_pos) const {
    return geometry_.widgetFromImage(image_pos);
  }
  QPoint imagePixelFromWidget(const QPoint& widget_pos) const {
    return geometry_.imagePixelFromWidget(widget_pos);
  }
  /// @}

  QSize sizeHint() const override;

 signals:
  /// Emitted whenever the zoom level changes (API, buttons, or Ctrl+wheel).
  void zoomChanged(double zoom_level);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  /**
   * @brief Overlay hook for subclasses, called after the image is painted.
   *
   * The painter operates in widget coordinates; use widgetFromImage() to
   * position overlay graphics so they stay crisp at any zoom level.
   */
  virtual void paintOverlay(QPainter& painter);

  /// Display geometry for subclasses needing direct access.
  const orc::gui::FrameViewGeometry& viewGeometry() const { return geometry_; }

 private:
  /// Resize the widget to the zoomed display size and refresh geometry.
  void applyGeometry();

  /// Find the enclosing QScrollArea (if any) for zoom-at-cursor scrolling.
  QScrollArea* enclosingScrollArea() const;

  orc::gui::FrameViewGeometry geometry_;
  QImage image_;
  double min_zoom_ = 0.25;
  double max_zoom_ = 8.0;
};

#endif  // FRAME_VIEWPORT_WIDGET_H
