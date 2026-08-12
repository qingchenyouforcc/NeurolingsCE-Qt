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

#include <atomic>
#include <memory>
#include <set>
#include <QSet>
#include <QString>
#include "ManagerEnvironmentController.hpp"
#include "MascotTemplateStore.hpp"
#include "MascotSessionStore.hpp"
#include "shijima-qt/CodexAppServerModels.hpp"
#include "shijima-qt/CodexAppServerClient.hpp"
#include <QHash>
#include <QDateTime>

class MascotData;
class QScreen;
class ShijimaWidget;

struct ShijimaManagerRuntimeState {
    ManagerEnvironmentController environment;
    int mascotTimer = -1;
    int windowObserverTimer = -1;
    int idCounter = 0;
    MascotTemplateStore templates;
    QSet<QString> listItemsToRefresh;
    QString importOnShowPath;
    MascotSessionStore sessions;
    // The app-server client is owned by the GUI manager.  It is constructed
    // during manager setup but only starts a process after an explicit user
    // action from the Codex page.
    std::unique_ptr<CodexAppServerClient> codexClient;
    QHash<QString, QDateTime> codexBubbleDedupe;
    QString mascotsPath;
    QString mascotCachePath;
    bool cliRuntimeMode = false;
    bool silentStartupMode = false;
    std::atomic<bool> shuttingDown{false};
};
