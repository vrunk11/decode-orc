/*
 * File:        masklineconfigdialog.h
 * Module:      orc-gui
 * Purpose:     Configuration dialog for mask line stage (broadcast line entry)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef MASKLINECONFIGDIALOG_H
#define MASKLINECONFIGDIALOG_H

#include <amplitude_conversion.h>
#include <orc/stage/common_types.h>

#include <QComboBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <optional>

#include "configdialogbase.h"
#include "presenters/include/hints_view_models.h"

/**
 * @brief Configuration dialog for the mask line stage
 *
 * Lets the user specify lines to mask using broadcast interlaced line numbering
 * (PAL: 1–625, NTSC/PAL-M: 1–525).  Field 1 occupies the lower half of that
 * range and field 2 the upper half; entering lines from either or both halves
 * masks the corresponding frame-flat lines in the CVBS frame buffer.
 *
 * Line numbers are converted from 1-based broadcast to 0-based frame-flat on
 * apply: broadcast line N → frame-flat line N-1.
 */
class MaskLineConfigDialog : public ConfigDialogBase {
  Q_OBJECT

 public:
  explicit MaskLineConfigDialog(QWidget* parent = nullptr);
  ~MaskLineConfigDialog() override = default;

  // Called from mainwindow to adapt mask-level display labels.
  void setAmplitudeUnit(orc::AmplitudeDisplayUnit unit);

  // Called from mainwindow; sets the valid line-number range and mask-level
  // presets for the detected video system.
  void setVideoParameters(
      const std::optional<orc::presenters::VideoParametersView>& params);

 protected:
  void apply_configuration() override;
  void load_from_parameters(
      const std::map<std::string, orc::ParameterValue>& params) override;

 private slots:
  void on_add_range();
  void on_remove_selected();
  void on_clear_all();
  void on_mask_level_preset_changed(int index);

 private:
  int field1_line_count() const;
  int max_frame_lines() const;
  void update_spinbox_limits();
  void update_mask_level_labels();
  void resolve_video_levels(int32_t& blanking, int32_t& white) const;
  std::string build_line_spec_from_ui() const;
  void parse_line_spec_to_ui(const std::string& line_spec);

  // Line entry
  QSpinBox* start_line_spinbox_;
  QSpinBox* end_line_spinbox_;
  QPushButton* add_button_;
  QListWidget* ranges_list_;

  // Mask level
  QComboBox* mask_level_preset_combo_;
  QSpinBox* mask_level_spinbox_;

  std::optional<orc::presenters::VideoParametersView> cached_video_params_;
};

#endif  // MASKLINECONFIGDIALOG_H
