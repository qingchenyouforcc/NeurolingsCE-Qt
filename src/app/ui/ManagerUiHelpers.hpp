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

#include <functional>
#include <memory>
#include <QString>

class QColor;
class QListWidget;
class ManagerTrayController;
class ShijimaManager;
class QWidget;

namespace ShijimaManagerUiInternal {

QString colorToString(QColor const& color);
void applyMascotListTheme(QListWidget& listWidget);
void refreshTrayMenu(ManagerTrayController *controller);
void setupTrayIcon(ShijimaManager *manager,
    std::unique_ptr<ManagerTrayController>& controller);
void teardownTrayIcon(std::unique_ptr<ManagerTrayController>& controller);
void showTrayMessage(QString const& title, QString const& message,
    ManagerTrayController *controller, std::function<void()> onClick = {});

// Blocking prompts use the project's ElaDialog/ElaPushButton chrome so close,
// warning and confirmation surfaces remain readable in every theme.  The
// helpers intentionally keep a conservative keyboard default: questions
// start on the cancel action and never make the affirmative action default.
bool showThemedQuestion(QWidget *parent, QString const& title,
    QString const& message, QString const& acceptText = {},
    QString const& cancelText = {}, bool destructive = false);
void showThemedInformation(QWidget *parent, QString const& title,
    QString const& message);
void showThemedWarning(QWidget *parent, QString const& title,
    QString const& message);
void showThemedInformationAsync(QWidget *parent, QString const& title,
    QString const& message);
bool showThemedTextInput(QWidget *parent, QString const& title,
    QString const& label, QString const& initial, QString *result,
    QString const& acceptText = {}, QString const& cancelText = {});

}
