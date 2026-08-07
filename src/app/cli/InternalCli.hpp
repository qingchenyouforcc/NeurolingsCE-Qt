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
#include "shijima-qt/MascotPackage.hpp"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

enum class CliCommandKind {
    Help,
    Version,
    DocumentList,
    DocumentSummon,
    DocumentClose,
    DocumentCloseAll,
    DocumentStop,
    DocumentMascot,
    CodexNotify,
    ListMascots,
    ListLoadedMascots,
    SpawnMascot,
    AlterMascot,
    DismissMascot,
    DismissAllMascots,
};

struct CliGlobalOptions {
    bool quiet = false;
    bool json = false;
    int connectTimeoutMs = 500;
    int readTimeoutMs = 500;
};

struct CliCommand {
    CliGlobalOptions global;
    CliCommandKind kind = CliCommandKind::Help;
    bool documentStyle = false;
    QString commandName;
    QString idToken;
    QString summonMode;
    QString mascotAction;
    QString mascotArchivePath;
    QString mascotTemplateName;
    QString codexNotifyPayload;
    QString selector;
    QStringList selectors;
    QStringList behaviors;
    bool sortById = false;
    std::optional<int> cliLabel;
    SpawnMascotRequest spawnRequest;
    MascotPatch patch;
};

struct CliError {
    QString code;
    QString error;
    QString details;
    QString usage;
    int exitCode = 1;
    int httpStatus = 0;
};

struct CliExecutionResult {
    QList<MascotInfo> mascots;
    QList<LoadedMascotInfo> loadedMascots;
    std::optional<MascotInfo> mascot;
    std::optional<MascotPackageReport> mascotValidation;
    QString removedTemplateName;
    bool codexHandled = false;
    QString codexEventType;
    QString codexState;
    std::optional<CliError> error;
};

struct CliParseResult {
    CliGlobalOptions global;
    CliCommand command;
    CliError error;
    bool hasCommand = false;
    bool hasError = false;
};

bool isCliInvocation(int argc, char **argv);
CliParseResult parseCliArguments(int argc, char **argv);
CliExecutionResult executeCliCommand(CliCommand const& command);
int writeCliOutput(CliCommand const& command, CliExecutionResult const& result);
int writeCliError(CliGlobalOptions const& global, CliError const& error);
