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

#include "InternalCli.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <iostream>
#include <vector>

#ifndef APP_NAME
#define APP_NAME "NeurolingsCE"
#endif

#ifndef NEUROLINGSCE_VERSION
#define NEUROLINGSCE_VERSION "0.3.0"
#endif

namespace {

QString appName() {
    return QStringLiteral(APP_NAME);
}

QString appVersion() {
    return QStringLiteral(NEUROLINGSCE_VERSION);
}

QString helpText() {
    return QStringLiteral(
        "%1 CLI\n"
        "\n"
        "Document commands:\n"
        "  --help, -h\n"
        "      Show help information.\n"
        "  --summon, -s mascot --name NAME [label]\n"
        "      Summon a specific mascot by name.\n"
        "  --summon, -s mascot --data-id ID [label]\n"
        "      Summon a specific mascot by data ID.\n"
        "  --summon, -s random [label]\n"
        "      Summon a random loaded mascot.\n"
        "  --close LABEL\n"
        "      Close the mascot mapped to the user label.\n"
        "  --close-all\n"
        "      Close all running mascots.\n"
        "  --stop\n"
        "      Close all mascots and stop the NeurolingsCE runtime.\n"
        "  --mascot, -m list\n"
        "      List loaded mascot templates.\n"
        "  --mascot, -m add ZIP\n"
        "      Import mascot templates from a zip archive.\n"
        "  --mascot, -m remove MASCOT\n"
        "      Remove a loaded mascot template by name.\n"
        "  --list, -l\n"
        "      List running mascots.\n"
        "  --version, -v\n"
        "      Show version information.\n"
        "\n"
        "Global options:\n"
        "  --quiet\n"
        "  --json\n"
        "  --connect-timeout-ms MS\n"
        "  --read-timeout-ms MS\n"
        "\n"
        "Notes:\n"
        "  - Labels are user-facing IDs and are separate from runtime mascot IDs.\n"
        "  - Labels are kept in memory only for the current %1 process.\n"
        "  - --mascot template management works standalone without a running %1 instance.\n"
        "  - Runtime commands auto-start %1 when no local runtime is ready.\n"
        "  - --host and --port are no longer supported.\n"
        "\n"
        "Legacy commands remain supported:\n"
        "  list, list-loaded, spawn, alter, dismiss, dismiss-all")
        .arg(appName());
}

QJsonObject helpJson() {
    QJsonObject object;
    object["app"] = appName();
    object["version"] = appVersion();

    QJsonArray globals;
    for (auto const& option : {
        QStringLiteral("--quiet"),
        QStringLiteral("--json"),
        QStringLiteral("--connect-timeout-ms"),
        QStringLiteral("--read-timeout-ms"),
    }) {
        globals.append(option);
    }
    object["global_options"] = globals;

    QJsonArray commands;
    auto appendCommand = [&](QString const& name, QJsonArray aliases,
        QString const& usage, QString const& description)
    {
        QJsonObject command;
        command["name"] = name;
        command["aliases"] = aliases;
        command["usage"] = usage;
        command["description"] = description;
        commands.append(command);
    };

    appendCommand(QStringLiteral("--help"), QJsonArray { QStringLiteral("-h") },
        QStringLiteral("--help|-h"),
        QStringLiteral("Show help information."));
    appendCommand(QStringLiteral("--summon"), QJsonArray { QStringLiteral("-s") },
        QStringLiteral("--summon mascot --name NAME [label] | "
            "--summon mascot --data-id ID [label] | "
            "--summon random [label]"),
        QStringLiteral("Summon a mascot and optionally assign a user label."));
    appendCommand(QStringLiteral("--close"), QJsonArray {},
        QStringLiteral("--close LABEL"),
        QStringLiteral("Close the mascot mapped to the user label."));
    appendCommand(QStringLiteral("--close-all"), QJsonArray {},
        QStringLiteral("--close-all"),
        QStringLiteral("Close all running mascots."));
    appendCommand(QStringLiteral("--stop"), QJsonArray {},
        QStringLiteral("--stop"),
        QStringLiteral("Close all mascots and stop the NeurolingsCE runtime."));
    appendCommand(QStringLiteral("--mascot"), QJsonArray { QStringLiteral("-m") },
        QStringLiteral("--mascot list | --mascot add ZIP | --mascot remove MASCOT"),
        QStringLiteral("List, import, or remove mascot templates."));
    appendCommand(QStringLiteral("--list"), QJsonArray { QStringLiteral("-l") },
        QStringLiteral("--list|-l"),
        QStringLiteral("List running mascots and their labels."));
    appendCommand(QStringLiteral("--version"), QJsonArray { QStringLiteral("-v") },
        QStringLiteral("--version|-v"),
        QStringLiteral("Show version information."));

    object["commands"] = commands;

    QJsonArray legacy;
    for (auto const& command : {
        QStringLiteral("list"),
        QStringLiteral("list-loaded"),
        QStringLiteral("spawn"),
        QStringLiteral("alter"),
        QStringLiteral("dismiss"),
        QStringLiteral("dismiss-all"),
    }) {
        legacy.append(command);
    }
    object["legacy_commands"] = legacy;
    object["label_scope"] = QStringLiteral("current_app_run");
    return object;
}

void printMascot(MascotInfo const& mascot) {
    std::cout << "[" << mascot.id << "] "
        << mascot.name.toStdString() << std::endl;
    std::cout << "  Data ID: " << mascot.dataId << std::endl;
    std::cout << "  Active behavior: "
        << mascot.activeBehavior.value_or(QString {}).toStdString() << std::endl;
    std::cout << "  Anchor: {" << mascot.anchorX << ", "
        << mascot.anchorY << "}" << std::endl;
}

void printDocumentMascotLine(MascotInfo const& mascot) {
    QString labelText = mascot.cliLabel.has_value()
        ? QString::number(mascot.cliLabel.value())
        : QStringLiteral("-");
    std::cout << "[label:" << labelText.toStdString() << "] "
        << "[runtime:" << mascot.id << "] "
        << mascot.name.toStdString() << std::endl;
}

QJsonObject buildDocumentMascotJson(CliCommand const& command,
    CliExecutionResult const& result)
{
    QJsonObject object;
    if (command.mascotAction == QStringLiteral("remove")) {
        object["removed"] = result.removedTemplateName.isEmpty()
            ? command.mascotTemplateName
            : result.removedTemplateName;
        return object;
    }

    QJsonArray array;
    for (auto const& mascot : result.loadedMascots) {
        array.append(loadedMascotInfoToJson(mascot));
    }
    object["templates"] = array;
    return object;
}

QJsonObject buildListJson(CliExecutionResult const& result) {
    QJsonObject object;
    QJsonArray array;
    for (auto const& mascot : result.mascots) {
        array.append(mascotInfoToJson(mascot));
    }
    object["mascots"] = array;
    return object;
}

QJsonObject buildLoadedListJson(CliExecutionResult const& result) {
    QJsonObject object;
    QJsonArray array;
    for (auto const& mascot : result.loadedMascots) {
        array.append(loadedMascotInfoToJson(mascot));
    }
    object["loaded_mascots"] = array;
    return object;
}

QJsonObject buildCommandJson(CliCommand const& command,
    CliExecutionResult const& result)
{
    if (command.kind == CliCommandKind::DocumentList ||
        command.kind == CliCommandKind::ListMascots)
    {
        return buildListJson(result);
    }
    if (command.kind == CliCommandKind::ListLoadedMascots) {
        return buildLoadedListJson(result);
    }
    if (command.kind == CliCommandKind::DocumentMascot) {
        return buildDocumentMascotJson(command, result);
    }
    if (result.mascot.has_value()) {
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(result.mascot.value());
        if (command.kind == CliCommandKind::DocumentSummon &&
            result.mascot->cliLabel.has_value())
        {
            object["label"] = result.mascot->cliLabel.value();
        }
        return object;
    }
    if (command.kind == CliCommandKind::DocumentStop) {
        QJsonObject object;
        object["stopped"] = true;
        return object;
    }
    return QJsonObject {};
}

QJsonObject successJson(CliCommand const& command,
    CliExecutionResult const& result)
{
    if (command.kind == CliCommandKind::Help) {
        return helpJson();
    }

    if (command.kind == CliCommandKind::Version) {
        QJsonObject object;
        object["app"] = appName();
        object["version"] = appVersion();
        return object;
    }
    return buildCommandJson(command, result);
}

QJsonObject errorJson(CliError const& error) {
    QJsonObject object;
    object["error"] = error.error;
    object["code"] = error.code;
    if (!error.details.isEmpty()) {
        object["details"] = error.details;
    }
    if (!error.usage.isEmpty()) {
        object["usage"] = error.usage;
    }
    if (error.httpStatus > 0) {
        object["status"] = error.httpStatus;
    }
    return object;
}

void writeJson(QJsonObject const& object) {
    QJsonDocument document(object);
    std::cout << document.toJson(QJsonDocument::Compact).constData()
        << std::endl;
}

void writeDocumentMascotText(CliCommand const& command,
    CliExecutionResult const& result)
{
    if (command.mascotAction == QStringLiteral("remove")) {
        std::cout << "Removed mascot template "
            << (result.removedTemplateName.isEmpty()
                ? command.mascotTemplateName
                : result.removedTemplateName).toStdString()
            << std::endl;
        return;
    }

    if (command.mascotAction == QStringLiteral("add")) {
        std::cout << "Imported mascot template(s):" << std::endl;
    }
    for (auto const& mascot : result.loadedMascots) {
        std::cout << "[" << mascot.id << "] "
            << mascot.name.toStdString() << std::endl;
        if (!mascot.version.isEmpty()) {
            std::cout << "  Version: "
                << mascot.version.toStdString() << std::endl;
        }
        if (!mascot.author.isEmpty()) {
            std::cout << "  Author: "
                << mascot.author.toStdString() << std::endl;
        }
    }
}

void writeStandardTextOutput(CliCommand const& command,
    CliExecutionResult const& result)
{
    if (command.kind == CliCommandKind::Help) {
        std::cout << helpText().toStdString() << std::endl;
        return;
    }
    if (command.kind == CliCommandKind::Version) {
        std::cout << appName().toStdString() << " "
            << appVersion().toStdString() << std::endl;
        return;
    }
    if (command.kind == CliCommandKind::DocumentList) {
        for (auto const& mascot : result.mascots) {
            printDocumentMascotLine(mascot);
        }
        return;
    }
    if (command.kind == CliCommandKind::DocumentSummon) {
        if (result.mascot.has_value()) {
            printDocumentMascotLine(result.mascot.value());
        }
        return;
    }
    if (command.kind == CliCommandKind::DocumentClose) {
        std::cout << "Closed label "
            << command.cliLabel.value_or(-1) << std::endl;
        return;
    }
    if (command.kind == CliCommandKind::DocumentCloseAll) {
        std::cout << "Closed all mascots" << std::endl;
        return;
    }
    if (command.kind == CliCommandKind::DocumentStop) {
        std::cout << "Stopped NeurolingsCE runtime" << std::endl;
        return;
    }
    if (command.kind == CliCommandKind::ListMascots) {
        for (auto const& mascot : result.mascots) {
            printMascot(mascot);
        }
        return;
    }
    if (command.kind == CliCommandKind::ListLoadedMascots) {
        std::vector<LoadedMascotInfo> mascots(result.loadedMascots.begin(),
            result.loadedMascots.end());
        if (command.sortById) {
            std::sort(mascots.begin(), mascots.end(),
                [](LoadedMascotInfo const& lhs, LoadedMascotInfo const& rhs) {
                    return lhs.id < rhs.id;
                });
        }
        for (auto const& mascot : mascots) {
            std::cout << "[" << mascot.id << "] "
                << mascot.name.toStdString() << std::endl;
            if (!mascot.version.isEmpty()) {
                std::cout << "  Version: "
                    << mascot.version.toStdString() << std::endl;
            }
            if (!mascot.author.isEmpty()) {
                std::cout << "  Author: "
                    << mascot.author.toStdString() << std::endl;
            }
        }
        return;
    }
    if (command.kind == CliCommandKind::DocumentMascot) {
        writeDocumentMascotText(command, result);
        return;
    }
    if (result.mascot.has_value()) {
        printMascot(result.mascot.value());
    }
}

}

int writeCliError(CliGlobalOptions const& global, CliError const& error) {
    if (global.json) {
        writeJson(errorJson(error));
    }
    else {
        std::cerr << "ERROR: " << error.error.toStdString() << std::endl;
        if (!error.details.isEmpty()) {
            std::cerr << error.details.toStdString() << std::endl;
        }
        if (!error.usage.isEmpty()) {
            std::cerr << error.usage.toStdString() << std::endl;
        }
    }
    return error.exitCode;
}

int writeCliOutput(CliCommand const& command, CliExecutionResult const& result) {
    if (result.error.has_value()) {
        return writeCliError(command.global, result.error.value());
    }

    if (command.global.quiet) {
        return EXIT_SUCCESS;
    }

    if (command.global.json) {
        writeJson(successJson(command, result));
        return EXIT_SUCCESS;
    }
    writeStandardTextOutput(command, result);
    return EXIT_SUCCESS;
}
