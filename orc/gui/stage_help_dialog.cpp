/*
 * File:        stage_help_dialog.cpp
 * Module:      orc-gui
 * Purpose:     Modal dialog for displaying stage help documentation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "stage_help_dialog.h"

#include <QDialogButtonBox>
#include <QSizePolicy>
#include <QVBoxLayout>

StageHelpDialog::StageHelpDialog(const QString& stage_display_name,
                                 const QString& markdown, QWidget* parent)
    : QDialog(parent), browser_(new QTextBrowser(this)) {
  setWindowTitle(QString("Help — %1").arg(stage_display_name));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(720, 560);
  setup_ui(markdown);
}

void StageHelpDialog::setup_ui(const QString& markdown) {
  browser_->document()->setDefaultStyleSheet(
      "h1 { margin-top: 16px; }"
      "h2 { margin-top: 14px; }"
      "h3 { margin-top: 10px; }");
  browser_->setOpenExternalLinks(false);
  browser_->setMarkdown(markdown);
  browser_->setReadOnly(true);
  browser_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);
  layout->addWidget(browser_);
  layout->addWidget(buttons);
  setLayout(layout);
}
