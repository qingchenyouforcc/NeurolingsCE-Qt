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

#include <QHash>
#include <QSet>

namespace {

struct ArgCursor {
    int argc = 0;
    char **argv = nullptr;
    int index = 1;

    bool hasNext() const {
        return index < argc;
    }

    QString peek() const {
        return hasNext() ? QString::fromUtf8(argv[index]) : QString {};
    }

    QString take() {
        return hasNext() ? QString::fromUtf8(argv[index++]) : QString {};
    }
};

QSet<QString> const& legacyCommands() {
    static const QSet<QString> commands = {
        QStringLiteral("list"),
        QStringLiteral("list-loaded"),
        QStringLiteral("spawn"),
        QStringLiteral("alter"),
        QStringLiteral("dismiss"),
        QStringLiteral("dismiss-all"),
    };
    return commands;
}

QSet<QString> const& documentCommands() {
    static const QSet<QString> commands = {
        QStringLiteral("--help"),
        QStringLiteral("-h"),
        QStringLiteral("--summon"),
        QStringLiteral("-s"),
        QStringLiteral("--close"),
        QStringLiteral("--close-all"),
        QStringLiteral("--stop"),
        QStringLiteral("--mascot"),
        QStringLiteral("-m"),
        QStringLiteral("--list"),
        QStringLiteral("-l"),
        QStringLiteral("--version"),
        QStringLiteral("-v"),
    };
    return commands;
}

bool isLegacyCommand(QString const& token) {
    return legacyCommands().contains(token);
}

bool isDocumentCommand(QString const& token) {
    return documentCommands().contains(token);
}

bool isBooleanGlobal(QString const& token) {
    return token == QStringLiteral("--quiet") || token == QStringLiteral("--json");
}

bool isValuedGlobal(QString const& token) {
    return token == QStringLiteral("--host") ||
        token == QStringLiteral("--port") ||
        token == QStringLiteral("--connect-timeout-ms") ||
        token == QStringLiteral("--read-timeout-ms");
}

QString helpSynopsis(char const *argv0) {
    return QStringLiteral(
        "Usage: %1 [--quiet] [--json] "
        "[--connect-timeout-ms MS] [--read-timeout-ms MS] <command>")
        .arg(QString::fromUtf8(argv0));
}

QString documentHelpText(char const *argv0) {
    QString executable = QString::fromUtf8(argv0);
    return QStringLiteral(
        "%1\n"
        "\n"
        "Document commands:\n"
        "  %2 --help|-h\n"
        "  %2 --version|-v\n"
        "  %2 --list|-l\n"
        "  %2 --summon|-s mascot --name NAME [label]\n"
        "  %2 --summon|-s mascot --data-id ID [label]\n"
        "  %2 --summon|-s random [label]\n"
        "  %2 --close LABEL\n"
        "  %2 --close-all\n"
        "  %2 --stop\n"
        "  %2 --mascot|-m list\n"
        "  %2 --mascot|-m add ZIP\n"
        "  %2 --mascot|-m remove MASCOT\n"
        "\n"
        "Global options:\n"
        "  --quiet  --json  --connect-timeout-ms MS  --read-timeout-ms MS\n"
        "\n"
        "Transport notes:\n"
        "  Runtime commands auto-start a local runtime when needed.\n"
        "  Commands use local IPC and do not use HTTP.\n"
        "  --host and --port are no longer supported.\n"
        "\n"
        "Legacy commands remain supported:\n"
        "  list, list-loaded, spawn, alter, dismiss, dismiss-all")
        .arg(helpSynopsis(argv0), executable);
}

QString commandUsage(char const *argv0, QString const& commandName) {
    QString executable = QString::fromUtf8(argv0);

    static const QHash<QString, QString> usages = {
        { QStringLiteral("list"),
            QStringLiteral("Usage: %1 [globals...] list [--selector JS] [--json]") },
        { QStringLiteral("list-loaded"),
            QStringLiteral("Usage: %1 [globals...] list-loaded [--sort-by-id] [--json]") },
        { QStringLiteral("spawn"),
            QStringLiteral(
                "Usage: %1 [globals...] spawn (--name NAME | --data-id ID) "
                "[--behavior NAME]... [--x X --y Y] [--json]") },
        { QStringLiteral("alter"),
            QStringLiteral(
                "Usage: %1 [globals...] alter --id ID_OR_AUTO [--selector JS]... "
                "[--behavior NAME]... [--x X --y Y] [--json]") },
        { QStringLiteral("dismiss"),
            QStringLiteral("Usage: %1 [globals...] dismiss --id ID_OR_AUTO [--selector JS]") },
        { QStringLiteral("dismiss-all"),
            QStringLiteral("Usage: %1 [globals...] dismiss-all [--selector JS]") },
        { QStringLiteral("--summon"),
            QStringLiteral(
                "Usage: %1 [globals...] --summon|-s mascot (--name NAME | --data-id ID) [label]\n"
                "       %1 [globals...] --summon|-s random [label]") },
        { QStringLiteral("--close"),
            QStringLiteral("Usage: %1 [globals...] --close LABEL") },
        { QStringLiteral("--close-all"),
            QStringLiteral("Usage: %1 [globals...] --close-all") },
        { QStringLiteral("--stop"),
            QStringLiteral("Usage: %1 [globals...] --stop") },
        { QStringLiteral("--mascot"),
            QStringLiteral(
                "Usage: %1 [globals...] --mascot|-m list\n"
                "       %1 [globals...] --mascot|-m add ZIP\n"
                "       %1 [globals...] --mascot|-m remove MASCOT") },
        { QStringLiteral("--list"),
            QStringLiteral("Usage: %1 [globals...] --list|-l") },
        { QStringLiteral("--version"),
            QStringLiteral("Usage: %1 [globals...] --version|-v") },
    };

    auto it = usages.constFind(commandName);
    if (it == usages.cend()) {
        return documentHelpText(argv0);
    }
    return it.value().arg(executable);
}

CliError parseError(CliGlobalOptions const&, QString const& message,
    char const *argv0, QString const& commandName = {},
    QString const& details = {})
{
    CliError error;
    error.code = QStringLiteral("invalid_arguments");
    error.error = message;
    error.details = details;
    error.usage = commandName.isEmpty() ? documentHelpText(argv0)
        : commandUsage(argv0, commandName);
    error.exitCode = 2;
    return error;
}

CliParseResult failParse(CliParseResult result, QString const& message,
    char const *argv0, QString const& commandName = {},
    QString const& details = {})
{
    result.error = parseError(result.global, message, argv0, commandName, details);
    result.hasError = true;
    return result;
}

bool parseIntValue(QString const& value, int &out) {
    bool ok = false;
    int parsed = value.toInt(&ok);
    if (!ok) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseDoubleValue(QString const& value, double &out) {
    bool ok = false;
    double parsed = value.toDouble(&ok);
    if (!ok) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseOptionalLabel(QString const& value, std::optional<int> &out) {
    int label = 0;
    if (!parseIntValue(value, label) || label < 0) {
        return false;
    }
    out = label;
    return true;
}

void setBooleanGlobal(QString const& token, CliGlobalOptions &global) {
    if (token == QStringLiteral("--quiet")) {
        global.quiet = true;
    }
    else if (token == QStringLiteral("--json")) {
        global.json = true;
    }
}

bool applyGlobalOption(QString const& token, QString const& value,
    CliGlobalOptions &global)
{
    if (token == QStringLiteral("--host") || token == QStringLiteral("--port")) {
        (void)value;
        return true;
    }
    if (token == QStringLiteral("--connect-timeout-ms")) {
        int timeout = 0;
        if (!parseIntValue(value, timeout) || timeout < 0) {
            return false;
        }
        global.connectTimeoutMs = timeout;
        return true;
    }
    if (token == QStringLiteral("--read-timeout-ms")) {
        int timeout = 0;
        if (!parseIntValue(value, timeout) || timeout < 0) {
            return false;
        }
        global.readTimeoutMs = timeout;
        return true;
    }
    return false;
}

bool validateAnchor(MascotPatch const& patch) {
    return !patch.hasAnchor() || patch.hasCompleteAnchor();
}

CliCommandKind documentCommandKind(QString const& token) {
    if (token == QStringLiteral("--help") || token == QStringLiteral("-h")) {
        return CliCommandKind::Help;
    }
    if (token == QStringLiteral("--version") || token == QStringLiteral("-v")) {
        return CliCommandKind::Version;
    }
    if (token == QStringLiteral("--list") || token == QStringLiteral("-l")) {
        return CliCommandKind::DocumentList;
    }
    if (token == QStringLiteral("--summon") || token == QStringLiteral("-s")) {
        return CliCommandKind::DocumentSummon;
    }
    if (token == QStringLiteral("--close")) {
        return CliCommandKind::DocumentClose;
    }
    if (token == QStringLiteral("--stop")) {
        return CliCommandKind::DocumentStop;
    }
    if (token == QStringLiteral("--mascot") || token == QStringLiteral("-m")) {
        return CliCommandKind::DocumentMascot;
    }
    return CliCommandKind::DocumentCloseAll;
}

CliCommandKind legacyCommandKind(QString const& token) {
    if (token == QStringLiteral("list")) {
        return CliCommandKind::ListMascots;
    }
    if (token == QStringLiteral("list-loaded")) {
        return CliCommandKind::ListLoadedMascots;
    }
    if (token == QStringLiteral("spawn")) {
        return CliCommandKind::SpawnMascot;
    }
    if (token == QStringLiteral("alter")) {
        return CliCommandKind::AlterMascot;
    }
    if (token == QStringLiteral("dismiss")) {
        return CliCommandKind::DismissMascot;
    }
    return CliCommandKind::DismissAllMascots;
}

CliParseResult parseDocumentMascotCommand(ArgCursor &args, CliParseResult result,
    QString const& commandToken, char const *argv0)
{
    CliCommand &command = result.command;

    if (!args.hasNext()) {
        return failParse(result, QStringLiteral("Missing mascot command"),
            argv0, commandToken);
    }

    command.mascotAction = args.take();

    if (command.mascotAction == QStringLiteral("list")) {
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
        result.global = command.global;
        return result;
    }

    if (command.mascotAction == QStringLiteral("add")) {
        if (!args.hasNext()) {
            return failParse(result, QStringLiteral("Missing mascot archive path"),
                argv0, commandToken);
        }
        command.mascotArchivePath = args.take();
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
        result.global = command.global;
        return result;
    }

    if (command.mascotAction == QStringLiteral("remove")) {
        if (!args.hasNext()) {
            return failParse(result, QStringLiteral("Missing mascot template name"),
                argv0, commandToken);
        }
        command.mascotTemplateName = args.take();
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
        result.global = command.global;
        return result;
    }

    return failParse(result,
        QStringLiteral("Mascot command must be list, add, or remove"),
        argv0, commandToken);
}

CliParseResult parseDocumentSummonCommand(ArgCursor &args, CliParseResult result,
    QString const& commandToken, char const *argv0)
{
    CliCommand &command = result.command;

    if (!args.hasNext()) {
        return failParse(result, QStringLiteral("Missing summon mode"),
            argv0, commandToken);
    }

    command.summonMode = args.take();
    if (command.summonMode != QStringLiteral("mascot") &&
        command.summonMode != QStringLiteral("random"))
    {
        return failParse(result,
            QStringLiteral("Summon mode must be mascot or random"),
            argv0, commandToken);
    }

    while (args.hasNext()) {
        QString token = args.peek();
        if (token == QStringLiteral("--name")) {
            args.take();
            if (!args.hasNext()) {
                return failParse(result, QStringLiteral("Missing value for --name"),
                    argv0, commandToken);
            }
            command.spawnRequest.name = args.take();
            continue;
        }
        if (token == QStringLiteral("--data-id")) {
            args.take();
            if (!args.hasNext()) {
                return failParse(result, QStringLiteral("Missing value for --data-id"),
                    argv0, commandToken);
            }
            int dataId = 0;
            if (!parseIntValue(args.take(), dataId)) {
                return failParse(result, QStringLiteral("Invalid value for --data-id"),
                    argv0, commandToken);
            }
            command.spawnRequest.dataId = dataId;
            continue;
        }

        std::optional<int> label;
        if (!parseOptionalLabel(token, label)) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(token),
                argv0, commandToken);
        }
        command.cliLabel = label;
        args.take();
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
    }

    if (command.summonMode == QStringLiteral("mascot")) {
        if (command.spawnRequest.name.has_value() ==
            command.spawnRequest.dataId.has_value())
        {
            return failParse(result,
                QStringLiteral("You must specify one of --name or --data-id"),
                argv0, commandToken);
        }
    }
    else if (command.spawnRequest.name.has_value() ||
        command.spawnRequest.dataId.has_value())
    {
        return failParse(result,
            QStringLiteral("random summon does not accept --name or --data-id"),
            argv0, commandToken);
    }

    result.global = command.global;
    return result;
}

CliParseResult parseDocumentCommand(ArgCursor &args, CliParseResult result,
    QString const& commandToken, char const *argv0)
{
    CliCommand &command = result.command;
    command.documentStyle = true;
    command.kind = documentCommandKind(commandToken);

    if (command.kind == CliCommandKind::Help ||
        command.kind == CliCommandKind::Version ||
        command.kind == CliCommandKind::DocumentList ||
        command.kind == CliCommandKind::DocumentCloseAll ||
        command.kind == CliCommandKind::DocumentStop)
    {
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
        result.global = command.global;
        return result;
    }

    if (command.kind == CliCommandKind::DocumentMascot) {
        return parseDocumentMascotCommand(args, result, commandToken, argv0);
    }

    if (command.kind == CliCommandKind::DocumentClose) {
        if (!args.hasNext()) {
            return failParse(result, QStringLiteral("Missing CLI label"),
                argv0, commandToken);
        }
        if (!parseOptionalLabel(args.take(), command.cliLabel)) {
            return failParse(result,
                QStringLiteral("CLI label must be a non-negative integer"),
                argv0, commandToken);
        }
        if (args.hasNext()) {
            return failParse(result,
                QStringLiteral("Unexpected argument: %1").arg(args.take()),
                argv0, commandToken);
        }
        result.global = command.global;
        return result;
    }

    return parseDocumentSummonCommand(args, result, commandToken, argv0);
}

CliParseResult parseLegacyCommand(ArgCursor &args, CliParseResult result,
    QString const& commandToken, char const *argv0)
{
    CliCommand &command = result.command;
    command.kind = legacyCommandKind(commandToken);

    auto requireValue = [&](QString const& option, QString &value) -> bool {
        if (!args.hasNext()) {
            result = failParse(result,
                QStringLiteral("Missing value for %1").arg(option),
                argv0, commandToken);
            return false;
        }
        value = args.take();
        return true;
    };

    while (args.hasNext()) {
        QString token = args.take();
        if (token == QStringLiteral("--json")) {
            command.global.json = true;
            continue;
        }

        if (command.kind == CliCommandKind::ListMascots) {
            if (token == QStringLiteral("--selector")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.selector = value;
                continue;
            }
        }
        else if (command.kind == CliCommandKind::ListLoadedMascots) {
            if (token == QStringLiteral("--sort-by-id")) {
                command.sortById = true;
                continue;
            }
        }
        else if (command.kind == CliCommandKind::SpawnMascot) {
            if (token == QStringLiteral("--name")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.spawnRequest.name = value;
                continue;
            }
            if (token == QStringLiteral("--data-id")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                int dataId = 0;
                if (!parseIntValue(value, dataId)) {
                    return failParse(result,
                        QStringLiteral("Invalid value for --data-id"),
                        argv0, commandToken);
                }
                command.spawnRequest.dataId = dataId;
                continue;
            }
            if (token == QStringLiteral("--behavior")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.behaviors.append(value);
                continue;
            }
            if (token == QStringLiteral("--x")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                double x = 0.0;
                if (!parseDoubleValue(value, x)) {
                    return failParse(result, QStringLiteral("Invalid value for --x"),
                        argv0, commandToken);
                }
                command.spawnRequest.patch.anchorX = x;
                continue;
            }
            if (token == QStringLiteral("--y")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                double y = 0.0;
                if (!parseDoubleValue(value, y)) {
                    return failParse(result, QStringLiteral("Invalid value for --y"),
                        argv0, commandToken);
                }
                command.spawnRequest.patch.anchorY = y;
                continue;
            }
        }
        else if (command.kind == CliCommandKind::AlterMascot) {
            if (token == QStringLiteral("--id")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.idToken = value;
                continue;
            }
            if (token == QStringLiteral("--selector")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.selectors.append(value);
                continue;
            }
            if (token == QStringLiteral("--behavior")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.behaviors.append(value);
                continue;
            }
            if (token == QStringLiteral("--x")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                double x = 0.0;
                if (!parseDoubleValue(value, x)) {
                    return failParse(result, QStringLiteral("Invalid value for --x"),
                        argv0, commandToken);
                }
                command.patch.anchorX = x;
                continue;
            }
            if (token == QStringLiteral("--y")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                double y = 0.0;
                if (!parseDoubleValue(value, y)) {
                    return failParse(result, QStringLiteral("Invalid value for --y"),
                        argv0, commandToken);
                }
                command.patch.anchorY = y;
                continue;
            }
        }
        else if (command.kind == CliCommandKind::DismissMascot) {
            if (token == QStringLiteral("--id")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.idToken = value;
                continue;
            }
            if (token == QStringLiteral("--selector")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.selector = value;
                continue;
            }
        }
        else if (command.kind == CliCommandKind::DismissAllMascots) {
            if (token == QStringLiteral("--selector")) {
                QString value;
                if (!requireValue(token, value)) {
                    return result;
                }
                command.selector = value;
                continue;
            }
        }

        return failParse(result,
            QStringLiteral("Unknown option: %1").arg(token),
            argv0, commandToken);
    }

    if (command.kind == CliCommandKind::ListLoadedMascots &&
        command.global.json && command.sortById)
    {
        return failParse(result,
            QStringLiteral("--json and --sort-by-id cannot be used together."),
            argv0, commandToken);
    }

    if (command.kind == CliCommandKind::SpawnMascot) {
        if (command.spawnRequest.name.has_value() ==
            command.spawnRequest.dataId.has_value())
        {
            return failParse(result,
                QStringLiteral("You must specify one of name or data-id."),
                argv0, commandToken);
        }
        if (!validateAnchor(command.spawnRequest.patch)) {
            return failParse(result, QStringLiteral("X and Y must be specified together"),
                argv0, commandToken);
        }
    }
    else if (command.kind == CliCommandKind::AlterMascot) {
        if (command.idToken.isEmpty()) {
            return failParse(result, QStringLiteral("Missing required option --id"),
                argv0, commandToken);
        }
        if (!validateAnchor(command.patch)) {
            return failParse(result, QStringLiteral("X and Y must be specified together"),
                argv0, commandToken);
        }
    }
    else if (command.kind == CliCommandKind::DismissMascot &&
        command.idToken.isEmpty())
    {
        return failParse(result, QStringLiteral("Missing required option --id"),
            argv0, commandToken);
    }

    result.global = command.global;
    return result;
}

}

bool isCliInvocation(int argc, char **argv) {
    if (argc <= 1) {
        return false;
    }
    int index = 1;
    while (index < argc) {
        QString token = QString::fromUtf8(argv[index]);
        if (isLegacyCommand(token) || isDocumentCommand(token)) {
            return true;
        }
        if (isBooleanGlobal(token)) {
            ++index;
            continue;
        }
        if (isValuedGlobal(token)) {
            if (index + 1 >= argc) {
                return false;
            }
            index += 2;
            continue;
        }
        return false;
    }
    return false;
}

CliParseResult parseCliArguments(int argc, char **argv) {
    CliParseResult result {};
    ArgCursor args { argc, argv, 1 };

    while (args.hasNext()) {
        QString token = args.peek();
        if (isLegacyCommand(token) || isDocumentCommand(token)) {
            break;
        }
        token = args.take();
        if (isBooleanGlobal(token)) {
            setBooleanGlobal(token, result.global);
            continue;
        }
        if (isValuedGlobal(token)) {
            if (!args.hasNext()) {
                return failParse(result,
                    QStringLiteral("Missing value for %1").arg(token),
                    argv[0]);
            }
            QString value = args.take();
            if (!applyGlobalOption(token, value, result.global)) {
                return failParse(result,
                    QStringLiteral("Invalid value for %1").arg(token),
                    argv[0]);
            }
            if (token == QStringLiteral("--host") || token == QStringLiteral("--port")) {
                return failParse(result,
                    QStringLiteral("%1 is not supported by the local IPC CLI")
                        .arg(token),
                    argv[0], {},
                    QStringLiteral("Use the local running instance instead of host/port routing."));
            }
            continue;
        }
        return failParse(result,
            QStringLiteral("Unknown global option: %1").arg(token),
            argv[0]);
    }

    if (!args.hasNext()) {
        return failParse(result, QStringLiteral("Missing command"), argv[0]);
    }

    QString commandToken = args.take();
    result.hasCommand = true;
    result.command.global = result.global;
    result.command.commandName = commandToken;

    if (isDocumentCommand(commandToken)) {
        return parseDocumentCommand(args, result, commandToken, argv[0]);
    }
    if (isLegacyCommand(commandToken)) {
        return parseLegacyCommand(args, result, commandToken, argv[0]);
    }

    return failParse(result,
        QStringLiteral("Unknown command: %1").arg(commandToken),
        argv[0]);
}
