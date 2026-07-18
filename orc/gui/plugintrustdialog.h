/*
 * File:        plugintrustdialog.h
 * Module:      orc-gui
 * Purpose:     Explicit trust confirmation for plugin binaries
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ORC_GUI_PLUGINTRUSTDIALOG_H
#define ORC_GUI_PLUGINTRUSTDIALOG_H

#include <QDialog>
#include <QString>

namespace orc {

/**
 * @brief Modal dialog confirming that the user trusts a plugin binary.
 *
 * Trust is a distinct decision from adding or enabling a plugin: this dialog
 * surfaces the source, license and digest status so the user can make it
 * deliberately. It supports an optional "add without trusting" outcome for the
 * add flows, where an entry may be recorded but left untrusted (and therefore
 * not downloaded or loaded) until trusted later.
 */
class PluginTrustDialog : public QDialog {
  Q_OBJECT

 public:
  enum class Choice { Cancel, AddUntrusted, Trust };

  struct Details {
    QString headline;       ///< Short summary line.
    QString source;         ///< Publisher / source repo / URL.
    QString license;        ///< SPDX license identifier.
    QString digest_status;  ///< e.g. "sha256 present" / "no digest".
  };

  // allow_untrusted enables the third "Add without trusting" button (used by
  // the add flows); when false only Trust/Cancel are offered.
  PluginTrustDialog(QWidget* parent, const Details& details,
                    bool allow_untrusted);

  Choice choice() const { return choice_; }

 private:
  Choice choice_ = Choice::Cancel;
};

}  // namespace orc

#endif  // ORC_GUI_PLUGINTRUSTDIALOG_H
