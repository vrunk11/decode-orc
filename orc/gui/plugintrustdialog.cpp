/*
 * File:        plugintrustdialog.cpp
 * Module:      orc-gui
 * Purpose:     Explicit trust confirmation for plugin binaries
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "plugintrustdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace orc {

PluginTrustDialog::PluginTrustDialog(QWidget* parent, const Details& details,
                                     bool allow_untrusted)
    : QDialog(parent) {
  setWindowTitle("Confirm Plugin Trust");
  setModal(true);

  auto* layout = new QVBoxLayout(this);

  auto* headline = new QLabel(
      details.headline.isEmpty()
          ? QStringLiteral(
                "This plugin is native code that will run with the same "
                "privileges as Decode-Orc. Only trust plugins from sources you "
                "recognise.")
          : details.headline,
      this);
  headline->setWordWrap(true);
  layout->addWidget(headline);

  auto* form = new QFormLayout();
  form->addRow("Source:",
               new QLabel(details.source.isEmpty() ? QStringLiteral("(unknown)")
                                                   : details.source,
                          this));
  form->addRow("License:", new QLabel(details.license.isEmpty()
                                          ? QStringLiteral("(unspecified)")
                                          : details.license,
                                      this));
  form->addRow("Integrity:", new QLabel(details.digest_status.isEmpty()
                                            ? QStringLiteral("(no digest)")
                                            : details.digest_status,
                                        this));
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(this);
  auto* trust_button =
      buttons->addButton("Trust", QDialogButtonBox::AcceptRole);
  QPushButton* untrusted_button = nullptr;
  if (allow_untrusted) {
    untrusted_button = buttons->addButton("Add without trusting",
                                          QDialogButtonBox::ActionRole);
  }
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);

  connect(trust_button, &QPushButton::clicked, this, [this]() {
    choice_ = Choice::Trust;
    accept();
  });
  if (untrusted_button) {
    connect(untrusted_button, &QPushButton::clicked, this, [this]() {
      choice_ = Choice::AddUntrusted;
      accept();
    });
  }
  connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
    choice_ = Choice::Cancel;
    reject();
  });
}

}  // namespace orc
