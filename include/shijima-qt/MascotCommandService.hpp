#pragma once

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

#include "shijima-qt/MascotApi.hpp"
#include "shijima-qt/CodexActivity.hpp"

#include <QByteArray>
#include <QList>

class ShijimaManager;

class MascotCommandService {
private:
    ShijimaManager *m_manager;
public:
    explicit MascotCommandService(ShijimaManager *manager);

    MascotCommandStatus listMascots(ListMascotsRequest const& request,
        QList<MascotInfo> &out) const;
    MascotCommandStatus spawnMascot(SpawnMascotRequest const& request,
        MascotInfo &out) const;
    MascotCommandStatus registerCliLabel(RegisterCliLabelRequest const& request,
        CliLabelInfo &out) const;
    MascotCommandStatus getCliLabel(int cliLabel, CliLabelInfo &out) const;
    MascotCommandStatus alterMascot(int mascotId, MascotPatch const& patch,
        MascotInfo &out) const;
    MascotCommandStatus getMascot(int mascotId, MascotInfo &out) const;
    MascotCommandStatus dismissMascot(int mascotId) const;
    MascotCommandStatus dismissAllMascots(
        DismissAllMascotsRequest const& request) const;
    MascotCommandStatus listLoadedMascots(QList<LoadedMascotInfo> &out) const;
    MascotCommandStatus importMascotTemplate(QString const& archivePath,
        QList<LoadedMascotInfo> &out) const;
    MascotCommandStatus removeMascotTemplate(QString const& mascotName) const;
    MascotCommandStatus stopRuntime() const;
    MascotCommandStatus showManagerWindow() const;
    MascotCommandStatus showCodexNotification(CodexActivity const& activity) const;
    MascotCommandStatus getLoadedMascot(int mascotId,
        LoadedMascotInfo &out) const;
    MascotCommandStatus getLoadedMascotPreviewPng(int mascotId,
        QByteArray &pngBytes) const;
    ApiPingInfo ping() const;
};
