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

#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QWidget;

// Owns the Mascot Store page widgets. The page is wired by
// ShijimaManager::setupStorePage(); all network/install state lives in
// core (MascotStoreCoordinator) and GitHubAuthManager.
struct MascotStoreUi {
    QWidget *storePage = nullptr;
    QLineEdit *searchEdit = nullptr;
    QComboBox *tagFilter = nullptr;
    QPushButton *refreshButton = nullptr;
    QPushButton *submitButton = nullptr;
    QPushButton *loginButton = nullptr;
    QLabel *loginStatusLabel = nullptr;
    QLabel *storeStatusLabel = nullptr;
    QListWidget *entryList = nullptr;
    QPushButton *detailButton = nullptr;
    QPushButton *installButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QWidget *loginHintLabel = nullptr;
};
