/*
 * File:        videoparameterobserverdialog.cpp
 * Module:      orc-gui
 * Purpose:     Video parameter observer dialog implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "videoparameterobserverdialog.h"

#include <QGridLayout>
#include <QVBoxLayout>

VideoParameterObserverDialog::VideoParameterObserverDialog(QWidget* parent)
    : QDialog(parent) {
  setupUI();
  setWindowTitle("Video Parameter Observer");
  setWindowFlags(Qt::Window);
  setAttribute(Qt::WA_DeleteOnClose, false);
  resize(480, 560);
  setMinimumSize(400, 480);
}

VideoParameterObserverDialog::~VideoParameterObserverDialog() = default;

void VideoParameterObserverDialog::setupUI() {
  auto* main = new QVBoxLayout(this);

  // --- Signal Parameters group (once per frame) ---
  signal_group_ = new QGroupBox("Signal Parameters", this);
  auto* sg = new QGridLayout(signal_group_);
  sg->setColumnStretch(1, 1);
  sg->setVerticalSpacing(6);
  sg->setHorizontalSpacing(12);

  int row = 0;
  sg->addWidget(new QLabel("System:"), row, 0);
  system_label_ = new QLabel("-");
  sg->addWidget(system_label_, row++, 1);

  sg->addWidget(new QLabel("Frame width:"), row, 0);
  frame_width_label_ = new QLabel("-");
  sg->addWidget(frame_width_label_, row++, 1);

  sg->addWidget(new QLabel("Burst range:"), row, 0);
  burst_range_label_ = new QLabel("-");
  burst_range_label_->setToolTip("Colour burst sample range");
  sg->addWidget(burst_range_label_, row++, 1);

  sg->addWidget(new QLabel("Active range:"), row, 0);
  active_range_label_ = new QLabel("-");
  active_range_label_->setToolTip("Active video sample range");
  sg->addWidget(active_range_label_, row++, 1);

  sg->addWidget(new QLabel("Levels (sync/blk/wht):"), row, 0);
  levels_label_ = new QLabel("-");
  levels_label_->setToolTip("Sync tip / black / white levels in 10-bit ADU");
  sg->addWidget(levels_label_, row++, 1);

  main->addWidget(signal_group_);

  // --- Field 1 group ---
  field1_group_ = new QGroupBox("Field 1 Observations", this);
  auto* f1 = new QGridLayout(field1_group_);
  f1->setColumnStretch(1, 1);
  f1->setVerticalSpacing(6);
  f1->setHorizontalSpacing(12);

  row = 0;
  f1->addWidget(new QLabel("Colour frame index:"), row, 0);
  field1_colour_frame_label_ = new QLabel("-");
  field1_colour_frame_label_->setToolTip(
      "Position in colour subcarrier sequence (1–4 PAL/PAL-M, 0–1 NTSC)");
  f1->addWidget(field1_colour_frame_label_, row++, 1);

  f1->addWidget(new QLabel("Burst level:"), row, 0);
  field1_burst_label_ = new QLabel("-");
  field1_burst_label_->setToolTip("Median colour burst amplitude (10-bit ADU)");
  f1->addWidget(field1_burst_label_, row++, 1);

  f1->addWidget(new QLabel("White SNR:"), row, 0);
  field1_snr_label_ = new QLabel("-");
  field1_snr_label_->setToolTip("Luminance signal-to-noise ratio");
  f1->addWidget(field1_snr_label_, row++, 1);

  f1->addWidget(new QLabel("Black PSNR:"), row, 0);
  field1_psnr_label_ = new QLabel("-");
  field1_psnr_label_->setToolTip("Black level peak signal-to-noise ratio");
  f1->addWidget(field1_psnr_label_, row++, 1);

  f1->addWidget(new QLabel("Quality score:"), row, 0);
  field1_quality_label_ = new QLabel("-");
  field1_quality_label_->setToolTip(
      "Disc quality score (0.0 = worst, 1.0 = best)");
  f1->addWidget(field1_quality_label_, row++, 1);

  f1->addWidget(new QLabel("Dropouts:"), row, 0);
  field1_dropout_label_ = new QLabel("-");
  field1_dropout_label_->setToolTip("Dropout sample count for this field");
  f1->addWidget(field1_dropout_label_, row++, 1);

  main->addWidget(field1_group_);

  // --- Field 2 group ---
  field2_group_ = new QGroupBox("Field 2 Observations", this);
  auto* f2 = new QGridLayout(field2_group_);
  f2->setColumnStretch(1, 1);
  f2->setVerticalSpacing(6);
  f2->setHorizontalSpacing(12);

  row = 0;
  f2->addWidget(new QLabel("Colour frame index:"), row, 0);
  field2_colour_frame_label_ = new QLabel("-");
  f2->addWidget(field2_colour_frame_label_, row++, 1);

  f2->addWidget(new QLabel("Burst level:"), row, 0);
  field2_burst_label_ = new QLabel("-");
  f2->addWidget(field2_burst_label_, row++, 1);

  f2->addWidget(new QLabel("White SNR:"), row, 0);
  field2_snr_label_ = new QLabel("-");
  f2->addWidget(field2_snr_label_, row++, 1);

  f2->addWidget(new QLabel("Black PSNR:"), row, 0);
  field2_psnr_label_ = new QLabel("-");
  f2->addWidget(field2_psnr_label_, row++, 1);

  f2->addWidget(new QLabel("Quality score:"), row, 0);
  field2_quality_label_ = new QLabel("-");
  f2->addWidget(field2_quality_label_, row++, 1);

  f2->addWidget(new QLabel("Dropouts:"), row, 0);
  field2_dropout_label_ = new QLabel("-");
  f2->addWidget(field2_dropout_label_, row++, 1);

  field2_group_->setVisible(false);
  main->addWidget(field2_group_);
  main->addStretch();
}

// ---------------------------------------------------------------------------
// Public update methods
// ---------------------------------------------------------------------------

void VideoParameterObserverDialog::updateObservations(
    const orc::FieldID& field_id,
    const orc::presenters::VideoParameterObservationView& obs) {
  (void)field_id;
  updateSignalParams(obs);
  field1_group_->setTitle("Field Observations");
  updateFieldGroup(field1_group_, field1_colour_frame_label_,
                   field1_burst_label_, field1_snr_label_, field1_psnr_label_,
                   field1_quality_label_, field1_dropout_label_, obs);
  field2_group_->setVisible(false);
}

void VideoParameterObserverDialog::updateObservationsForFrame(
    const orc::FieldID& field1_id,
    const orc::presenters::VideoParameterObservationView& field1_obs,
    const orc::FieldID& field2_id,
    const orc::presenters::VideoParameterObservationView& field2_obs) {
  (void)field1_id;
  (void)field2_id;
  updateSignalParams(field1_obs);
  field1_group_->setTitle("Field 1 Observations");
  updateFieldGroup(field1_group_, field1_colour_frame_label_,
                   field1_burst_label_, field1_snr_label_, field1_psnr_label_,
                   field1_quality_label_, field1_dropout_label_, field1_obs);
  field2_group_->setVisible(true);
  updateFieldGroup(field2_group_, field2_colour_frame_label_,
                   field2_burst_label_, field2_snr_label_, field2_psnr_label_,
                   field2_quality_label_, field2_dropout_label_, field2_obs);
}

void VideoParameterObserverDialog::clearObservations() {
  system_label_->setText("-");
  frame_width_label_->setText("-");
  burst_range_label_->setText("-");
  active_range_label_->setText("-");
  levels_label_->setText("-");

  const auto clear = [](QLabel* l) { l->setText("-"); };
  clear(field1_colour_frame_label_);
  clear(field1_burst_label_);
  clear(field1_snr_label_);
  clear(field1_psnr_label_);
  clear(field1_quality_label_);
  clear(field1_dropout_label_);
  clear(field2_colour_frame_label_);
  clear(field2_burst_label_);
  clear(field2_snr_label_);
  clear(field2_psnr_label_);
  clear(field2_quality_label_);
  clear(field2_dropout_label_);
  field2_group_->setVisible(false);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void VideoParameterObserverDialog::updateSignalParams(
    const orc::presenters::VideoParameterObservationView& obs) {
  if (!obs.video_params.has_value()) {
    system_label_->setText("-");
    frame_width_label_->setText("-");
    burst_range_label_->setText("-");
    active_range_label_->setText("-");
    levels_label_->setText("-");
    return;
  }
  const auto& vp = *obs.video_params;
  system_label_->setText(systemName(vp.system));
  frame_width_label_->setText(vp.frame_width_nominal >= 0
                                  ? QString::number(vp.frame_width_nominal) +
                                        " samples/line"
                                  : "-");
  burst_range_label_->setText(
      (vp.color_burst_start >= 0 && vp.color_burst_end >= 0)
          ? QString("%1–%2").arg(vp.color_burst_start).arg(vp.color_burst_end)
          : "-");
  active_range_label_->setText(
      (vp.active_video_start >= 0 && vp.active_video_end >= 0)
          ? QString("%1–%2").arg(vp.active_video_start).arg(vp.active_video_end)
          : "-");
  levels_label_->setText(
      (vp.sync_tip_level >= 0 && vp.black_level >= 0 && vp.white_level >= 0)
          ? QString("%1 / %2 / %3")
                .arg(vp.sync_tip_level)
                .arg(vp.black_level)
                .arg(vp.white_level)
          : "-");
}

void VideoParameterObserverDialog::updateFieldGroup(
    QGroupBox* /*group*/, QLabel* colour_frame_label, QLabel* burst_label,
    QLabel* snr_label, QLabel* psnr_label, QLabel* quality_label,
    QLabel* dropout_label,
    const orc::presenters::VideoParameterObservationView& obs) {
  if (obs.colour_frame_index.has_value()) {
    const int idx = *obs.colour_frame_index;
    colour_frame_label->setText(idx >= 0 ? QString::number(idx) : "unknown");
  } else {
    colour_frame_label->setText("-");
  }

  burst_label->setText(obs.burst_level_10bit.has_value()
                           ? QString::number(*obs.burst_level_10bit, 'f', 1) +
                                 " ADU"
                           : "-");
  snr_label->setText(fmtOptDouble(obs.white_snr_db, " dB"));
  psnr_label->setText(fmtOptDouble(obs.black_psnr_db, " dB"));

  if (obs.quality_score.has_value()) {
    quality_label->setText(QString::number(*obs.quality_score, 'f', 3));
  } else {
    quality_label->setText("-");
  }

  dropout_label->setText(obs.dropout_count.has_value()
                             ? QString::number(*obs.dropout_count)
                             : "-");
}

QString VideoParameterObserverDialog::systemName(
    orc::presenters::VideoSystem sys) {
  switch (sys) {
    case orc::presenters::VideoSystem::PAL:
      return "PAL";
    case orc::presenters::VideoSystem::NTSC:
      return "NTSC";
    case orc::presenters::VideoSystem::PAL_M:
      return "PAL-M";
    default:
      return "Unknown";
  }
}

QString VideoParameterObserverDialog::fmtOptDouble(
    const std::optional<double>& v, const char* unit) {
  if (!v.has_value()) return "-";
  return QString::number(*v, 'f', 1) + unit;
}
