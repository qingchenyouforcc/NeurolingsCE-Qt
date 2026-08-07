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

#include <memory>

#include <QColor>
#include <QString>

#include "ManagerTrayController.hpp"
#include "interface/MascotStoreUi.hpp"

class QAction;
class QLabel;
class QListWidget;
class QPushButton;
class QTranslator;
class QWidget;

struct ShijimaManagerUiState {
    QColor sandboxBackground;
    QAction *windowedModeAction = nullptr;
    QWidget *sandboxWidget = nullptr;
    QListWidget *listWidget = nullptr;
    QTranslator *translator = nullptr;
    QTranslator *qtTranslator = nullptr;
    QString currentLanguage = "en";
    QLabel *statusLabel = nullptr;
    QWidget *mascotDetailsPanel = nullptr;
    QLabel *mascotPreviewLabel = nullptr;
    QLabel *mascotNameLabel = nullptr;
    QLabel *mascotVersionLabel = nullptr;
    QLabel *mascotAuthorLabel = nullptr;
    QLabel *mascotDescriptionLabel = nullptr;
    QWidget *mascotEmptyStateWidget = nullptr;
    QWidget *spawnRandomButton = nullptr;
    QWidget *deleteMascotButton = nullptr;
    QWidget *homePage = nullptr;
    QWidget *createPage = nullptr;
    QWidget *combinationsPage = nullptr;
    QListWidget *combinationListWidget = nullptr;
    QLabel *combinationDetailsLabel = nullptr;
    QPushButton *restoreCombinationButton = nullptr;
    QPushButton *deleteCombinationButton = nullptr;
    QWidget *settingsPage = nullptr;
    std::unique_ptr<MascotStoreUi> storeUi;
    std::unique_ptr<ManagerTrayController> trayController;
    QString settingsKey;
    QString aboutKey;
};
