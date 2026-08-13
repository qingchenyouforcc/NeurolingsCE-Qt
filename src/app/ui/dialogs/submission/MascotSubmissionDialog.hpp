//
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <QDialog>

class ElaCheckBox;
class ElaLineEdit;
class ElaPlainTextEdit;
class QLabel;
class QPushButton;
class GitHubAuthManager;
class MascotSubmissionClient;

class MascotSubmissionDialog final : public QDialog
{
    Q_OBJECT
public:
    MascotSubmissionDialog(GitHubAuthManager *auth,
        MascotSubmissionClient *client, QWidget *parent = nullptr);

private slots:
    void pickPackage();
    void submit();

private:
    void setBusy(bool busy);

    GitHubAuthManager *m_auth = nullptr;
    MascotSubmissionClient *m_client = nullptr;
    ElaLineEdit *m_packagePath = nullptr;
    QPushButton *m_pickButton = nullptr;
    ElaLineEdit *m_id = nullptr;
    ElaLineEdit *m_name = nullptr;
    ElaLineEdit *m_version = nullptr;
    ElaLineEdit *m_summary = nullptr;
    ElaPlainTextEdit *m_description = nullptr;
    ElaLineEdit *m_license = nullptr;
    ElaLineEdit *m_maintainers = nullptr;
    ElaCheckBox *m_rightsConfirmed = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_submitButton = nullptr;
    QLabel *m_prLink = nullptr;
};
