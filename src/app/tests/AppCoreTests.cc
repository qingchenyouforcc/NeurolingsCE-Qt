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

#include "../core/commands/MascotCommandDispatcher.hpp"
#include "../cli/InternalCli.hpp"
#include "shijima-qt/MascotApi.hpp"
#include "shijima-qt/CodexActivity.hpp"
#include "shijima-qt/CodexConfigManager.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/SafePath.hpp"
#include "shijima-qt/SecurityLimits.hpp"
#include "core/shijima-engine/shijima/broadcast/manager.hpp"
#include "core/shijima-engine/shijima/behavior/manager.hpp"
#include "core/shijima-engine/shijima/mascot/manager.hpp"
#include "core/shijima-engine/shijima/scripting/context.hpp"
#include "ui/mascot/MascotHoldGesture.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool condition, char const *message) {
    if (condition) {
        return;
    }
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

quint32 testCrc32(QByteArray const& bytes) {
    static quint32 table[256] = {};
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0xedb88320U ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }

    quint32 c = 0xffffffffU;
    for (auto ch : bytes) {
        c = table[(c ^ static_cast<quint8>(ch)) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffU;
}

void testWrite16(QFile &file, quint16 value) {
    char data[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
    };
    file.write(data, 2);
}

void testWrite32(QFile &file, quint32 value) {
    char data[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    file.write(data, 4);
}

struct TestZipEntry {
    QString name;
    QByteArray data;
    quint32 crc = 0;
    quint32 offset = 0;
};

void testWriteZip(QString const& path, std::vector<TestZipEntry> entries) {
    QFile file(path);
    expect(file.open(QFile::WriteOnly | QFile::Truncate),
        "test zip should be writable");
    for (auto &entry : entries) {
        QByteArray name = entry.name.toUtf8();
        entry.crc = testCrc32(entry.data);
        entry.offset = static_cast<quint32>(file.pos());
        testWrite32(file, 0x04034b50);
        testWrite16(file, 20);
        testWrite16(file, 0x0800);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite32(file, entry.crc);
        testWrite32(file, static_cast<quint32>(entry.data.size()));
        testWrite32(file, static_cast<quint32>(entry.data.size()));
        testWrite16(file, static_cast<quint16>(name.size()));
        testWrite16(file, 0);
        file.write(name);
        file.write(entry.data);
    }
    quint32 centralOffset = static_cast<quint32>(file.pos());
    for (auto const& entry : entries) {
        QByteArray name = entry.name.toUtf8();
        testWrite32(file, 0x02014b50);
        testWrite16(file, 20);
        testWrite16(file, 20);
        testWrite16(file, 0x0800);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite32(file, entry.crc);
        testWrite32(file, static_cast<quint32>(entry.data.size()));
        testWrite32(file, static_cast<quint32>(entry.data.size()));
        testWrite16(file, static_cast<quint16>(name.size()));
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite16(file, 0);
        testWrite32(file, 0);
        testWrite32(file, entry.offset);
        file.write(name);
    }
    quint32 centralSize = static_cast<quint32>(file.pos()) - centralOffset;
    testWrite32(file, 0x06054b50);
    testWrite16(file, 0);
    testWrite16(file, 0);
    testWrite16(file, static_cast<quint16>(entries.size()));
    testWrite16(file, static_cast<quint16>(entries.size()));
    testWrite32(file, centralSize);
    testWrite32(file, centralOffset);
    testWrite16(file, 0);
}

QByteArray minimalActionsXml() {
    return QByteArrayLiteral(
        "<Mascot><ActionList><Action Name=\"Look\"><Animation>"
        "<Pose Image=\"shime1.png\" /></Animation></Action></ActionList></Mascot>");
}

QByteArray minimalBehaviorsXml() {
    return QByteArrayLiteral("<Mascot><BehaviorList /></Mascot>");
}

QByteArray minimalPngBytes() {
    static constexpr unsigned char bytes[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
        0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
        0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99,
        0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };
    return QByteArray(reinterpret_cast<char const *>(bytes),
        static_cast<qsizetype>(sizeof(bytes)));
}

QByteArray oversizedHeaderOnlyPngBytes() {
    static constexpr unsigned char bytes[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00
    };
    return QByteArray(reinterpret_cast<char const *>(bytes),
        static_cast<qsizetype>(sizeof(bytes)));
}

struct FakeMascotService {
    QString lastSelector;
    QString importedArchive;
    QString removedTemplate;
    int alteredMascotId = -1;
    int dismissedMascotId = -1;

    MascotCommandStatus listMascots(ListMascotsRequest const& request,
        QList<MascotInfo> &out)
    {
        lastSelector = request.selector;
        out.append(MascotInfo { 7, 3, QStringLiteral("Default"),
            QStringLiteral("Fall"), 2, 12.5, 42.0 });
        return MascotCommandStatus::success();
    }

    MascotCommandStatus spawnMascot(SpawnMascotRequest const& request,
        MascotInfo &out)
    {
        out.id = 8;
        out.dataId = request.dataId.value_or(3);
        out.name = request.name.value_or(QStringLiteral("Default"));
        out.anchorX = request.patch.anchorX.value_or(0.0);
        out.anchorY = request.patch.anchorY.value_or(0.0);
        out.activeBehavior = request.patch.behavior;
        return MascotCommandStatus::success();
    }

    MascotCommandStatus registerCliLabel(RegisterCliLabelRequest const& request,
        CliLabelInfo &out)
    {
        out.mascotId = request.mascotId;
        out.label = request.label.value_or(5);
        return MascotCommandStatus::success();
    }

    MascotCommandStatus getCliLabel(int cliLabel, CliLabelInfo &out) {
        out.label = cliLabel;
        out.mascotId = 8;
        return MascotCommandStatus::success();
    }

    MascotCommandStatus alterMascot(int mascotId, MascotPatch const& patch,
        MascotInfo &out)
    {
        alteredMascotId = mascotId;
        out.id = mascotId;
        out.dataId = 3;
        out.name = QStringLiteral("Default");
        out.anchorX = patch.anchorX.value_or(0.0);
        out.anchorY = patch.anchorY.value_or(0.0);
        out.activeBehavior = patch.behavior;
        return MascotCommandStatus::success();
    }

    MascotCommandStatus getMascot(int, MascotInfo &) {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus dismissMascot(int mascotId) {
        dismissedMascotId = mascotId;
        return MascotCommandStatus::success();
    }

    MascotCommandStatus dismissAllMascots(DismissAllMascotsRequest const&) {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus listLoadedMascots(QList<LoadedMascotInfo> &out) {
        out.append(LoadedMascotInfo { 3, QStringLiteral("Default"),
            QStringLiteral("1.0"), QStringLiteral("Bundled"), QStringLiteral("pixelomer") });
        return MascotCommandStatus::success();
    }

    MascotCommandStatus importMascotTemplate(QString const& archivePath,
        QList<LoadedMascotInfo> &out)
    {
        importedArchive = archivePath;
        out.append(LoadedMascotInfo { 4, QStringLiteral("Imported") });
        return MascotCommandStatus::success();
    }

    MascotCommandStatus removeMascotTemplate(QString const& mascotName) {
        removedTemplate = mascotName;
        return MascotCommandStatus::success();
    }

    MascotCommandStatus stopRuntime() {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus showManagerWindow() {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus showCodexNotification(CodexActivity const&) {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus getLoadedMascot(int, LoadedMascotInfo &) {
        return MascotCommandStatus::success();
    }

    MascotCommandStatus getLoadedMascotPreviewPng(int, QByteArray &) {
        return MascotCommandStatus::success();
    }

    ApiPingInfo ping() const {
        ApiPingInfo info;
        info.ok = true;
        info.app = QStringLiteral("NeurolingsCE");
        info.apiVersion = QStringLiteral("v1");
        return info;
    }
};

void testMascotPatchParsing() {
    MascotPatch patch;
    QString error;
    expect(!mascotPatchFromJson(QJsonObject {
        { QStringLiteral("anchor"), QJsonObject {
            { QStringLiteral("x"), 12.0 },
        } },
    }, patch, &error), "partial anchor should be rejected");

    expect(mascotPatchFromJson(QJsonObject {
        { QStringLiteral("anchor"), QJsonObject {
            { QStringLiteral("x"), 12.0 },
            { QStringLiteral("y"), 34.0 },
        } },
        { QStringLiteral("behavior"), QStringLiteral("Fall") },
    }, patch, &error), "complete patch should parse");
    expect(patch.hasCompleteAnchor(), "complete patch should have anchor");
    expect(patch.behavior.value_or(QString()) == QStringLiteral("Fall"),
        "patch should preserve behavior");
}

void testJsonRoundTrips() {
    MascotInfo mascot;
    mascot.id = 10;
    mascot.dataId = 2;
    mascot.name = QStringLiteral("Default");
    mascot.activeBehavior = QStringLiteral("Sit");
    mascot.cliLabel = 4;
    mascot.anchorX = 1.5;
    mascot.anchorY = 2.5;

    MascotInfo parsedMascot;
    QString error;
    expect(mascotInfoFromJson(mascotInfoToJson(mascot), parsedMascot, &error),
        "mascot info should round-trip");
    expect(parsedMascot.id == mascot.id && parsedMascot.dataId == mascot.dataId,
        "mascot IDs should round-trip");
    expect(parsedMascot.activeBehavior == mascot.activeBehavior,
        "active behavior should round-trip");
    expect(parsedMascot.cliLabel == mascot.cliLabel,
        "CLI label should round-trip");

    LoadedMascotInfo loaded { 3, QStringLiteral("Jenny"),
        QStringLiteral("1.2"), QStringLiteral("Test mascot"),
        QStringLiteral("author") };
    LoadedMascotInfo parsedLoaded;
    expect(loadedMascotInfoFromJson(loadedMascotInfoToJson(loaded), parsedLoaded,
        &error), "loaded mascot info should round-trip");
    expect(parsedLoaded.name == loaded.name && parsedLoaded.version == loaded.version,
        "loaded mascot metadata should round-trip");
}

void testStatusJson() {
    auto success = MascotCommandStatus::success();
    expect(success.ok(), "success status should be OK");

    auto failure = MascotCommandStatus::failure(404,
        QStringLiteral("missing"), QStringLiteral("Not found"));
    auto json = errorToJson(failure);
    expect(!failure.ok(), "failure status should not be OK");
    expect(json.value(QStringLiteral("status")).toInt() == 404,
        "failure JSON should include status");
    expect(json.value(QStringLiteral("code")).toString() == QStringLiteral("missing"),
        "failure JSON should include code");
}

void testMascotPackageNames() {
    expect(MascotPackage::sanitizedPackageBaseName(QStringLiteral("  Bad<>:\"/\\|?*.  ")) ==
        QStringLiteral("Bad_________"), "package name should strip invalid characters and trailing dots");
    expect(MascotPackage::sanitizedPackageBaseName(QString()) ==
        QStringLiteral("Mascot"), "empty package name should use fallback");
    expect(MascotPackage::sanitizedPackageBaseName(QStringLiteral("../Name")) ==
        QStringLiteral(".._Name"), "path separators should not survive package names");
    expect(MascotPackage::isValidPackageName(QStringLiteral("Friendly Mascot")),
        "ordinary mascot names should be valid package names");
    expect(!MascotPackage::isValidPackageName(QStringLiteral("CON")),
        "Windows reserved device names should be rejected");
    expect(!MascotPackage::isValidPackageName(QString(201, QLatin1Char('a'))),
        "overlong package names should be rejected");
}

void testCodexActivityParsing() {
    CodexActivity activity;
    bool recognized = false;
    QString error;
    expect(codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("agent-turn-complete") },
        { QStringLiteral("thread-id"), QStringLiteral("thread") },
        { QStringLiteral("turn-id"), QStringLiteral("turn") },
        { QStringLiteral("cwd"), QStringLiteral("C:/work") },
        { QStringLiteral("input-messages"), QJsonArray {
            QStringLiteral("user input") } },
        { QStringLiteral("last-assistant-message"), QStringLiteral("😀 done") },
    }, activity, &recognized, &error), "Codex completion should parse");
    expect(recognized && activity.state == CodexActivityState::Ready,
        "Codex completion should map to Ready");
    expect(activity.lastAssistantMessage == QStringLiteral("😀 done"),
        "Codex parser should keep only the final assistant message for display");

    recognized = false;
    error.clear();
    expect(codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("session-title-updated") },
        { QStringLiteral("thread-id"), QStringLiteral("new-thread") },
        { QStringLiteral("title"), QStringLiteral("Markdown bubble support") },
        { QStringLiteral("description"), QStringLiteral("The new session is ready.") },
    }, activity, &recognized, &error),
        "Codex new-session title notifications should parse");
    expect(recognized && activity.isNewSession &&
        activity.sessionTitle == QStringLiteral("Markdown bubble support") &&
        activity.sessionDescription == QStringLiteral("The new session is ready."),
        "Codex new-session notifications should retain title and description");

    recognized = false;
    error.clear();
    expect(codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("thread/name/updated") },
        { QStringLiteral("params"), QJsonObject {
            { QStringLiteral("threadName"), QStringLiteral("Nested title") },
            { QStringLiteral("description"), QStringLiteral("Nested description") },
        } },
    }, activity, &recognized, &error) && recognized && activity.isNewSession &&
        activity.sessionTitle == QStringLiteral("Nested title"),
        "Codex title updates should accept app-server-shaped nested fields");

    recognized = false;
    error.clear();
    expect(codexActivityFromJson(QJsonObject {
        { QStringLiteral("method"), QStringLiteral("thread/name/updated") },
        { QStringLiteral("params"), QJsonObject {
            { QStringLiteral("threadName"), QStringLiteral("JSON-RPC title") },
        } },
    }, activity, &recognized, &error) && recognized && activity.isNewSession &&
        activity.sessionTitle == QStringLiteral("JSON-RPC title"),
        "Codex title updates should accept JSON-RPC method notifications");

    auto titleJson = codexActivityToJson(activity);
    expect(titleJson.value(QStringLiteral("title")).toString() ==
        QStringLiteral("JSON-RPC title"),
        "Codex activity serialization should retain a new-session title");

    expect(!codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("session-title-updated") },
        { QStringLiteral("title"), 42 },
    }, activity, &recognized, &error),
        "Malformed Codex title notifications should be rejected");

    expect(truncateCodexGraphemes(QStringLiteral("😀áb"), 2) ==
        QStringLiteral("😀á…"), "Codex excerpts should preserve grapheme clusters");

    expect(normalizeCodexBubbleText(QStringLiteral("  a\r\n\r\r\nb  ")) ==
        QStringLiteral("a\n\nb"),
        "Codex bubble text should normalize line endings and blank lines");
    QString longText = QStringLiteral("BEGIN\n")
        + QString(5000, QChar(0x4e2d))
        + QStringLiteral("\nFINAL\nobject\\NeurolingsCE }\n"
            "::git-push{... branch=\"main\"}");
    auto compact = compactCodexBubbleSource(longText);
    expect(compact.truncated && compact.retainedGraphemes <= 4096,
        "long Codex bubble sources should be bounded by grapheme budget");
    expect(compact.text.startsWith(QStringLiteral("BEGIN")) &&
        compact.text.endsWith(QStringLiteral("…")) &&
        !compact.text.contains(QStringLiteral("FINAL")) &&
        !compact.text.contains(QStringLiteral("object\\NeurolingsCE")) &&
        !compact.text.contains(QStringLiteral("::git-push")),
        "long Codex bubble sources should retain only the beginning before an ellipsis");
    expect(compactCodexBubbleSource(QStringLiteral("abcdef"), 0).text.isEmpty(),
        "zero Codex grapheme budget should produce an empty excerpt");
    auto one = compactCodexBubbleSource(QStringLiteral("😀abcdef"), 1);
    expect(one.truncated && one.retainedGraphemes == 1 &&
        one.text.startsWith(QStringLiteral("😀")),
        "one-grapheme Codex budget should preserve a complete first grapheme");
    auto two = compactCodexBubbleSource(QStringLiteral("abcdef"), 2);
    expect(two.truncated && two.retainedGraphemes == 2 &&
        two.text == QStringLiteral("ab…"),
        "two-grapheme Codex budget should retain a prefix safely");

    recognized = true;
    expect(codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("future-event") },
    }, activity, &recognized, &error) && !recognized,
        "Unknown Codex events should be accepted and ignored");
    expect(!codexActivityFromJson(QJsonObject {
        { QStringLiteral("type"), QStringLiteral("agent-turn-complete") },
        { QStringLiteral("last-assistant-message"), 42 },
    }, activity, &recognized, &error),
        "Malformed Codex completion fields should be rejected");
}

void testCodexCliParsing() {
    char argv0[] = "NeurolingsCE-cli";
    char option[] = "--codex-notify";
    QByteArray validPayload = QByteArrayLiteral(
        "{\"type\":\"agent-turn-complete\",\"last-assistant-message\":\"done\"}");
    char *validArgv[] = { argv0, option, validPayload.data() };
    expect(isCliInvocation(3, validArgv),
        "CLI invocation detection should recognize --codex-notify");
    auto valid = parseCliArguments(3, validArgv);
    expect(!valid.hasError && valid.hasCommand &&
        valid.command.kind == CliCommandKind::CodexNotify,
        "CLI should parse a Codex completion payload");
    expect(valid.command.codexNotifyPayload == QString::fromUtf8(validPayload),
        "CLI should preserve the Codex payload for dispatch");

    char jsonOption[] = "--json";
    char *jsonArgv[] = { argv0, option, validPayload.data(), jsonOption };
    auto json = parseCliArguments(4, jsonArgv);
    expect(!json.hasError && json.global.json,
        "CLI Codex notifications should accept --json");

    char *missingArgv[] = { argv0, option };
    auto missing = parseCliArguments(2, missingArgv);
    expect(missing.hasError && missing.error.error.contains(QStringLiteral("Missing")),
        "CLI should reject a missing Codex payload");

    char malformedPayload[] = "{broken";
    char *malformedArgv[] = { argv0, option, malformedPayload };
    auto malformed = parseCliArguments(3, malformedArgv);
    expect(malformed.hasError,
        "CLI should reject malformed Codex JSON");

    char unknownPayload[] = "{\"type\":\"future-event\"}";
    char *unknownArgv[] = { argv0, option, unknownPayload };
    auto unknown = parseCliArguments(3, unknownArgv);
    expect(!unknown.hasError && unknown.command.codexNotifyPayload ==
        QString::fromUtf8(unknownPayload),
        "CLI should accept unknown Codex event types for silent compatibility");

    QByteArray oversizedPayload(kCodexNotifyMaxBytes + 1, 'x');
    char *oversizedArgv[] = { argv0, option, oversizedPayload.data() };
    auto oversized = parseCliArguments(3, oversizedArgv);
    expect(oversized.hasError && oversized.error.error.contains(QStringLiteral("maximum size")),
        "CLI should reject oversized Codex payloads before parsing");

    char *multipleArgv[] = { argv0, option, validPayload.data(), malformedPayload };
    auto multiple = parseCliArguments(4, multipleArgv);
    expect(multiple.hasError,
        "CLI should reject multiple Codex payload arguments");
}

void testCodexConfigManagement() {
    QTemporaryDir temp;
    expect(temp.isValid(), "Codex config test root should be valid");
    QString path = QDir(temp.path()).absoluteFilePath(QStringLiteral("config.toml"));
    QString executable = QDir(temp.path()).absoluteFilePath(
        QStringLiteral("NeurolingsCE-cli.exe"));
    QFile executableFile(executable);
    expect(executableFile.open(QFile::WriteOnly),
        "Codex CLI fixture should be writable");
    executableFile.close();
    QFile initial(path);
    expect(initial.open(QFile::WriteOnly | QFile::Text),
        "Codex config fixture should be writable");
    initial.write("model = \"gpt\"\n\n");
    initial.close();

    auto enabled = enableCodexNotify(path, executable);
    expect(enabled.ok && enabled.changed, "Codex config should install managed block");
    QFile readBack(path);
    readBack.open(QFile::ReadOnly | QFile::Text);
    QString content = QString::fromUtf8(readBack.readAll());
    readBack.close();
    expect(content.contains(QStringLiteral("BEGIN NeurolingsCE Codex notify")) &&
        content.contains(QStringLiteral("--codex-notify")),
        "Codex managed block should contain the absolute CLI command");
    QString escapedExecutable = executable;
    escapedExecutable.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    expect(content.contains(escapedExecutable),
        "Codex managed block should escape Windows path separators for TOML");
    expect(!enabled.backupPath.isEmpty() && QFile::exists(enabled.backupPath),
        "Codex config changes should create a timestamped backup");

    auto idempotent = enableCodexNotify(path, executable);
    expect(idempotent.ok && !idempotent.changed,
        "re-enabling an unchanged Codex config should be idempotent");

    QString conflictPath = QDir(temp.path()).absoluteFilePath(QStringLiteral("conflict.toml"));
    QFile conflict(conflictPath);
    conflict.open(QFile::WriteOnly | QFile::Text);
    conflict.write("notify = [\"other\", \"--event\"]\n");
    conflict.close();
    auto rejected = enableCodexNotify(conflictPath, executable);
    expect(!rejected.ok && rejected.conflict,
        "Codex config should refuse to overwrite an unmanaged notify setting");

    QString computerUsePath = QDir(temp.path()).absoluteFilePath(
        QStringLiteral("computer-use.toml"));
    QString computerUseLine = QStringLiteral(
        "notify = [ \"C:\\\\Users\\\\tester\\\\AppData\\\\Local\\\\OpenAI\\\\Codex\\\\"
        "runtimes\\\\cua_node\\\\runtime\\\\bin\\\\codex-computer-use.exe\", "
        "\"turn-ended\" ]\n");
    QString computerUseOriginal = QStringLiteral("model = \"gpt\"\n") +
        computerUseLine + QStringLiteral("approval_policy = \"never\"\n");
    QFile computerUseConfig(computerUsePath);
    computerUseConfig.open(QFile::WriteOnly | QFile::Text);
    computerUseConfig.write(computerUseOriginal.toUtf8());
    computerUseConfig.close();

    auto bridged = enableCodexNotify(computerUsePath, executable);
    expect(bridged.ok && bridged.changed && bridged.bridgedExistingNotify,
        "Codex Desktop computer-use notify should be bridged safely");
    QFile bridgedRead(computerUsePath);
    bridgedRead.open(QFile::ReadOnly | QFile::Text);
    QString bridgedContent = QString::fromUtf8(bridgedRead.readAll());
    bridgedRead.close();
    expect(bridgedContent.count(QStringLiteral("notify =")) == 1 &&
        bridgedContent.contains(QStringLiteral("previous-notify-base64")) &&
        bridgedContent.contains(QStringLiteral("--codex-notify")),
        "bridging should preserve one active notify and encoded restore metadata");

    QStringList forwardCommand;
    QString forwardError;
    expect(loadCodexForwardNotifyCommand(computerUsePath, forwardCommand,
        &forwardError),
        "bridged computer-use command should be readable");
    expect(forwardCommand.size() == 2 &&
        forwardCommand.value(0).endsWith(
            QStringLiteral("codex-computer-use.exe"), Qt::CaseInsensitive) &&
        forwardCommand.value(1) == QStringLiteral("turn-ended"),
        "bridged command should preserve the original executable and argument");

    auto bridgeIdempotent = enableCodexNotify(computerUsePath, executable);
    expect(bridgeIdempotent.ok && !bridgeIdempotent.changed,
        "re-enabling a computer-use bridge should be idempotent");
    auto bridgeDisabled = disableCodexNotify(computerUsePath);
    expect(bridgeDisabled.ok && bridgeDisabled.changed,
        "disabling a computer-use bridge should restore the original callback");
    QFile bridgeAfterDisable(computerUsePath);
    bridgeAfterDisable.open(QFile::ReadOnly | QFile::Text);
    expect(QString::fromUtf8(bridgeAfterDisable.readAll()) == computerUseOriginal,
        "disabling should restore the computer-use notify line byte-for-byte");

    QString duplicatePath = QDir(temp.path()).absoluteFilePath(QStringLiteral("duplicate.toml"));
    QFile duplicate(duplicatePath);
    duplicate.open(QFile::WriteOnly | QFile::Text);
    duplicate.write((content + QStringLiteral(
        "notify = [\"D:/old/NeurolingsCE-cli.exe\", \"--codex-notify\"]\n")).toUtf8());
    duplicate.close();
    auto normalized = enableCodexNotify(duplicatePath, executable);
    expect(normalized.ok && normalized.changed,
        "a duplicate legacy NeurolingsCE notify should be removed beside its managed block");
    QFile normalizedRead(duplicatePath);
    normalizedRead.open(QFile::ReadOnly | QFile::Text);
    QString normalizedContent = QString::fromUtf8(normalizedRead.readAll());
    normalizedRead.close();
    expect(normalizedContent.count(QStringLiteral("notify =")) == 1 &&
        !normalizedContent.contains(QStringLiteral("D:/old/NeurolingsCE-cli.exe")),
        "normalizing a duplicate should leave one current managed command");

    QString legacyPath = QDir(temp.path()).absoluteFilePath(QStringLiteral("legacy.toml"));
    QFile legacy(legacyPath);
    legacy.open(QFile::WriteOnly | QFile::Text);
    legacy.write("model = \"gpt\"\n"
        "notify = [\"D:/CPP_project/NeurolingsCE/out/build/x64-Release/bin/"
        "NeurolingsCE-cli.exe\", \"--codex-notify\"]\n"
        "approval_policy = \"never\"\n");
    legacy.close();
    auto migrated = enableCodexNotify(legacyPath, executable);
    expect(migrated.ok && migrated.changed && !migrated.conflict,
        "an unmarked legacy NeurolingsCE notify should be migrated");
    QFile migratedRead(legacyPath);
    migratedRead.open(QFile::ReadOnly | QFile::Text);
    QString migratedContent = QString::fromUtf8(migratedRead.readAll());
    migratedRead.close();
    expect(migratedContent.count(QStringLiteral("notify =")) == 1 &&
        migratedContent.contains(QStringLiteral("BEGIN NeurolingsCE Codex notify")) &&
        migratedContent.contains(QStringLiteral("approval_policy = \"never\"")),
        "legacy migration should replace only the NeurolingsCE notify line");
    auto migratedDisabled = disableCodexNotify(legacyPath);
    expect(migratedDisabled.ok && migratedDisabled.changed,
        "a migrated legacy notify should be removable");
    QFile migratedAfterDisable(legacyPath);
    migratedAfterDisable.open(QFile::ReadOnly | QFile::Text);
    expect(QString::fromUtf8(migratedAfterDisable.readAll()) ==
        QStringLiteral("model = \"gpt\"\napproval_policy = \"never\"\n"),
        "removing a migrated legacy notify should preserve adjacent settings");

    auto disabled = disableCodexNotify(path);
    expect(disabled.ok && disabled.changed, "Codex config should remove only its managed block");
    QFile afterDisable(path);
    afterDisable.open(QFile::ReadOnly | QFile::Text);
    QString remaining = QString::fromUtf8(afterDisable.readAll());
    expect(remaining == QStringLiteral("model = \"gpt\"\n\n"),
        "disabling Codex should preserve unrelated settings byte-for-byte");

    QString noFinalNewlinePath = QDir(temp.path()).absoluteFilePath(
        QStringLiteral("no-final-newline.toml"));
    QFile noFinalNewline(noFinalNewlinePath);
    noFinalNewline.open(QFile::WriteOnly | QFile::Text);
    noFinalNewline.write("model = \"gpt\"");
    noFinalNewline.close();
    expect(enableCodexNotify(noFinalNewlinePath, executable).ok,
        "Codex config should support files without a final newline");
    expect(disableCodexNotify(noFinalNewlinePath).ok,
        "Codex config without a final newline should be removable");
    QFile noFinalNewlineRead(noFinalNewlinePath);
    noFinalNewlineRead.open(QFile::ReadOnly | QFile::Text);
    expect(QString::fromUtf8(noFinalNewlineRead.readAll()) ==
        QStringLiteral("model = \"gpt\""),
        "Codex uninstall should restore a file without a final newline");

    auto missingExecutable = enableCodexNotify(
        QDir(temp.path()).absoluteFilePath(QStringLiteral("missing-cli.toml")),
        QDir(temp.path()).absoluteFilePath(QStringLiteral("missing-cli.exe")));
    expect(!missingExecutable.ok &&
        missingExecutable.error.contains(QStringLiteral("was not found")),
        "Codex config should not install a callback to a missing CLI executable");
}

void testLegacyArchiveAnalysisAndConversion() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");
    QDir tempDir(temp.path());
    QString archivePath = tempDir.absoluteFilePath(QStringLiteral("legacy.zip"));
    QString outputPath = tempDir.absoluteFilePath(QStringLiteral("out"));
    QDir().mkpath(outputPath);

    QByteArray alphaInfoJson = QStringLiteral(
        "{\r\n"
        "  \"name\": \"Alpha Display\",\r\n"
        "  \"version\": \"1.0\",\r\n"
        "  \"description\": \"Alpha mascot description\",\r\n"
        "  \"author\": \"tester\",\r\n"
        "  \"sourceExtra\": 42,\r\n"
        "  \"separator\": \"line\u2028separator\"\r\n"
        "}\r\n").toUtf8();

    std::vector<TestZipEntry> entries {
        { QStringLiteral("Alpha/actions.xml"), minimalActionsXml() },
        { QStringLiteral("Alpha/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Alpha/img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("Alpha/img/a.png"), minimalPngBytes() },
        { QStringLiteral("Alpha/info.json"), alphaInfoJson },
        { QStringLiteral("Beta/actions.xml"), minimalActionsXml() },
        { QStringLiteral("Beta/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Beta/img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("Beta/img/cover.png"), minimalPngBytes() },
        { QStringLiteral("Invalid/actions.xml"), minimalActionsXml() },
        { QStringLiteral("Invalid/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Invalid/img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("Invalid/info.json"), QByteArrayLiteral("{broken") },
        { QStringLiteral("Broken/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Broken/img/shime1.png"), minimalPngBytes() },
    };
    testWriteZip(archivePath, entries);

    auto analysis = MascotPackage::analyzeLegacyArchive(archivePath);
    expect(analysis.ok, "legacy archive with valid candidates should be OK");
    expect(analysis.candidates.size() >= 4,
        "analysis should include valid and incomplete candidates");

    auto findCandidate = [&analysis](QString const& name) {
        auto it = std::find_if(analysis.candidates.begin(), analysis.candidates.end(),
            [&name](LegacyMascotCandidate const& candidate) {
                return candidate.sourceName == name || candidate.name == name ||
                    candidate.metadata.name == name;
            });
        return it == analysis.candidates.end() ? nullptr : &(*it);
    };

    auto *alpha = findCandidate(QStringLiteral("Alpha"));
    expect(alpha != nullptr && alpha->convertible,
        "valid candidate with metadata should be convertible");
    expect(alpha != nullptr && alpha->infoJsonValid,
        "valid candidate metadata should be marked as valid JSON");
    expect(alpha != nullptr && alpha->metadata.version == QStringLiteral("1.0") &&
        alpha->metadata.author == QStringLiteral("tester") &&
        alpha->metadata.description == QStringLiteral("Alpha mascot description"),
        "legacy analysis should preserve info.json metadata");
    expect(alpha != nullptr && alpha->sourceName == QStringLiteral("Alpha"),
        "legacy analysis should retain the archive source name");
    expect(alpha != nullptr && alpha->metadata.name == QStringLiteral("Alpha Display"),
        "legacy analysis should keep the metadata display name separate");
    expect(alpha != nullptr && alpha->infoJson.contains("\"sourceExtra\": 42"),
        "legacy analysis should preserve the complete info.json document");
    auto *beta = findCandidate(QStringLiteral("Beta"));
    expect(beta != nullptr && beta->convertible && beta->generatedMetadata,
        "candidate without info.json should use fallback metadata");
    expect(beta != nullptr && beta->infoJsonValid,
        "generated fallback metadata should be valid JSON");
    auto *invalid = findCandidate(QStringLiteral("Invalid"));
    expect(invalid != nullptr && invalid->convertible &&
        invalid->generatedMetadata && invalid->infoJson == QByteArrayLiteral("{broken"),
        "analysis should preserve invalid info.json content for editing");
    expect(invalid != nullptr && !invalid->infoJsonValid,
        "invalid existing info.json should require repair before conversion");
    expect(invalid != nullptr && !invalid->infoJsonError.isEmpty(),
        "invalid existing info.json should retain its validation reason");
    auto *broken = findCandidate(QStringLiteral("Broken"));
    expect(broken != nullptr && !broken->convertible &&
        broken->errors.join(QStringLiteral(";")).contains(QStringLiteral("actions.xml")),
        "candidate missing actions.xml should report a content error");

    QString untouchedOutputPath = tempDir.absoluteFilePath(
        QStringLiteral("untouched-out"));
    auto untouchedResults = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, untouchedOutputPath, QStringList { QStringLiteral("Alpha") });
    expect(untouchedResults.size() == 1 && untouchedResults.constFirst().ok,
        "untouched metadata should convert successfully through the legacy API");
    QString untouchedExtractPath = tempDir.absoluteFilePath(
        QStringLiteral("untouched-verify"));
    QString untouchedError;
    expect(MascotPackage::extractPackage(untouchedResults.constFirst().packagePath,
        untouchedExtractPath, untouchedError),
        "untouched package should extract for metadata verification");
    QFile untouchedInfo(QDir(untouchedExtractPath).absoluteFilePath(
        QStringLiteral("info.json")));
    bool untouchedInfoOpened = untouchedInfo.open(QFile::ReadOnly);
    expect(untouchedInfoOpened, "untouched package info.json should be readable");
    expect(untouchedInfoOpened && untouchedInfo.readAll() == alphaInfoJson,
        "untouched conversion should preserve info.json bytes exactly");

    QFile existing(QDir(outputPath).absoluteFilePath(
        QStringLiteral("Alpha Edited.mascot")));
    bool const existingOpened = existing.open(QFile::WriteOnly);
    expect(existingOpened, "existing package placeholder should be writable");
    if (existingOpened) {
        existing.write("existing");
        existing.close();
    }

    QByteArray editedAlphaInfoJson = QByteArrayLiteral(
        "{\n"
        "  \"name\": \"Alpha Edited\",\n"
        "  \"version\": \"2.0\",\n"
        "  \"description\": \"Edited Alpha description\",\n"
        "  \"author\": \"editor\",\n"
        "  \"custom\": \"kept\"\n"
        "}\n");
    QHash<QString, QByteArray> infoJsonOverrides {
        { QStringLiteral("Alpha"), editedAlphaInfoJson },
    };
    auto results = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, outputPath, QStringList {
            QStringLiteral("Alpha"),
            QStringLiteral("Beta"),
        }, infoJsonOverrides);
    expect(results.size() == 2, "selected legacy candidates should produce two results");
    expect(std::all_of(results.begin(), results.end(),
        [](LegacyMascotConversionResult const& result) { return result.ok; }),
        "selected valid candidates should convert successfully");

    bool alphaAvoidedOverwrite = std::any_of(results.begin(), results.end(),
        [](LegacyMascotConversionResult const& result) {
            return result.name == QStringLiteral("Alpha Edited") &&
                result.packagePath.endsWith(QStringLiteral("Alpha Edited-2.mascot"));
        });
    expect(alphaAvoidedOverwrite, "conversion should avoid overwriting existing packages");

    QByteArray invalidPackageNameJson = QByteArrayLiteral(
        "{\"name\":\"CON\",\"version\":\"1\"}");
    QHash<QString, QByteArray> invalidNameOverride {
        { QStringLiteral("Alpha"), invalidPackageNameJson },
    };
    auto invalidNameResults = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, tempDir.absoluteFilePath(QStringLiteral("invalid-name-out")),
        QStringList { QStringLiteral("Alpha") }, invalidNameOverride);
    expect(invalidNameResults.size() == 1 && !invalidNameResults.constFirst().ok &&
        invalidNameResults.constFirst().errorMessage == QStringLiteral(
            "Edited info.json has an invalid package name"),
        "edited metadata should reject non-portable package names");

    QHash<QString, QByteArray> invalidJsonOverride {
        { QStringLiteral("Alpha"), QByteArrayLiteral("{broken") },
    };
    auto invalidJsonResults = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, tempDir.absoluteFilePath(QStringLiteral("invalid-json-out")),
        QStringList { QStringLiteral("Alpha") }, invalidJsonOverride);
    expect(invalidJsonResults.size() == 1 && !invalidJsonResults.constFirst().ok &&
        invalidJsonResults.constFirst().errorMessage == QStringLiteral(
            "Edited info.json is invalid"),
        "edited metadata should reject malformed JSON");

    QByteArray oversizedInfoJson(static_cast<qsizetype>(
        SecurityLimits::kMascotSingleFileMaxBytes + 1), ' ');
    QHash<QString, QByteArray> oversizedOverride {
        { QStringLiteral("Alpha"), oversizedInfoJson },
    };
    auto oversizedResults = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, tempDir.absoluteFilePath(QStringLiteral("oversized-json-out")),
        QStringList { QStringLiteral("Alpha") }, oversizedOverride);
    expect(oversizedResults.size() == 1 && !oversizedResults.constFirst().ok &&
        oversizedResults.constFirst().errorMessage == QStringLiteral(
            "Edited info.json is too large"),
        "edited metadata should reject oversized JSON");

    for (auto const& result : results) {
        MascotMetadata metadata;
        QString error;
        expect(MascotPackage::inspectPackage(result.packagePath, metadata, error),
            "converted package should pass package inspection");
        if (result.name == QStringLiteral("Alpha Edited")) {
            expect(metadata.version == QStringLiteral("2.0") &&
                metadata.author == QStringLiteral("editor") &&
                metadata.description == QStringLiteral("Edited Alpha description"),
                "converted package should apply edited info.json metadata");
        }

        QString extractedPath = QDir(temp.path()).absoluteFilePath(
            QStringLiteral("verify-") + result.name);
        expect(MascotPackage::extractPackage(result.packagePath, extractedPath,
            error), "converted package should extract for preview verification");
        if (result.name == QStringLiteral("Alpha Edited")) {
            QFile extractedInfo(QDir(extractedPath).absoluteFilePath(
                QStringLiteral("info.json")));
            bool infoOpened = extractedInfo.open(QFile::ReadOnly);
            expect(infoOpened,
                "converted package info.json should be readable");
            expect(infoOpened &&
                extractedInfo.readAll().contains("\"custom\": \"kept\""),
                "converted package should preserve edited custom JSON fields");
        }
        QString previewName = result.name == QStringLiteral("Alpha Edited")
            ? QStringLiteral("a.png") : QStringLiteral("cover.png");
        expect(QFile::exists(QDir(extractedPath).absoluteFilePath(
            QStringLiteral("img/") + previewName)),
            "conversion should preserve the mascot GUI preview image");
    }
}

void testPackageInspectionRejectsOversizedPngHeader() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");
    QString packagePath = QDir(temp.path()).absoluteFilePath(
        QStringLiteral("oversized.mascot"));

    MascotMetadata metadata;
    metadata.name = QStringLiteral("Oversized");
    std::vector<TestZipEntry> entries {
        { QStringLiteral("info.json"), MascotPackage::metadataToJson(metadata) },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), oversizedHeaderOnlyPngBytes() },
    };
    testWriteZip(packagePath, entries);

    QString error;
    MascotMetadata parsed;
    expect(!MascotPackage::inspectPackage(packagePath, parsed, error),
        "package inspection should reject oversized PNG headers");
    expect(error.contains(QStringLiteral("maximum pixel count")) ||
        error.contains(QStringLiteral("invalid dimensions")),
        "oversized PNG rejection should mention dimensions");
}

void testPackageInspectionRejectsMalformedPng() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");
    QString packagePath = QDir(temp.path()).absoluteFilePath(
        QStringLiteral("malformed.mascot"));

    MascotMetadata metadata;
    metadata.name = QStringLiteral("Malformed");
    std::vector<TestZipEntry> entries {
        { QStringLiteral("info.json"), MascotPackage::metadataToJson(metadata) },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), QByteArrayLiteral("not a png") },
    };
    testWriteZip(packagePath, entries);

    QString error;
    MascotMetadata parsed;
    expect(!MascotPackage::inspectPackage(packagePath, parsed, error),
        "package inspection should reject malformed PNG files");
    expect(error.contains(QStringLiteral("not a valid PNG")),
        "malformed PNG rejection should explain the invalid format");
}

void testImportArchiveSupportsLegacyTemplateDirectory() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");

    QDir tempDir(temp.path());
    QString storagePath = tempDir.absoluteFilePath(QStringLiteral("storage"));
    expect(QDir().mkpath(storagePath), "storage directory should be creatable");

    QString templatePath = tempDir.absoluteFilePath(QStringLiteral("LegacyTemplate"));
    expect(QDir().mkpath(QDir(templatePath).absoluteFilePath(QStringLiteral("img"))),
        "legacy template image directory should be creatable");

    QFile actionsFile(QDir(templatePath).absoluteFilePath(QStringLiteral("actions.xml")));
    bool actionsOpened = actionsFile.open(QFile::WriteOnly | QFile::Truncate);
    expect(actionsOpened, "legacy template actions.xml should be writable");
    if (actionsOpened) {
        actionsFile.write(minimalActionsXml());
        actionsFile.close();
    }

    QFile behaviorsFile(QDir(templatePath).absoluteFilePath(
        QStringLiteral("behaviors.xml")));
    bool behaviorsOpened = behaviorsFile.open(QFile::WriteOnly | QFile::Truncate);
    expect(behaviorsOpened, "legacy template behaviors.xml should be writable");
    if (behaviorsOpened) {
        behaviorsFile.write(minimalBehaviorsXml());
        behaviorsFile.close();
    }

    QFile imageFile(QDir(templatePath).absoluteFilePath(QStringLiteral("img/shime1.png")));
    bool imageOpened = imageFile.open(QFile::WriteOnly | QFile::Truncate);
    expect(imageOpened, "legacy template shime1.png should be writable");
    if (imageOpened) {
        imageFile.write(minimalPngBytes());
        imageFile.close();
    }

    auto imported = MascotPackage::importArchive(templatePath, storagePath);
    expect(imported.size() == 1,
        "legacy template directory import should register one mascot package");
    if (imported.size() == 1) {
        QString importedName = QString::fromStdString(*imported.begin());
        expect(!importedName.isEmpty(),
            "legacy template directory import should return an installed package name");
        expect(QFile::exists(QDir(storagePath).absoluteFilePath(
            importedName + QStringLiteral(".mascot"))),
            "legacy template directory import should write a mascot package to storage");
    }
}

void testCommandDispatcher() {
    FakeMascotService service;

    auto missing = MascotCommandDispatcher::dispatchRequest({}, service);
    expect(missing.value(QStringLiteral("code")).toString() == QStringLiteral("bad_request"),
        "dispatcher should reject missing command");

    auto unknown = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("unknown") },
    }, service);
    expect(unknown.value(QStringLiteral("error")).toString() == QStringLiteral("Unknown command"),
        "dispatcher should reject unknown command");

    auto ping = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("ping") },
    }, service);
    expect(ping.value(QStringLiteral("ok")).toBool(false),
        "dispatcher should return ping payload");

    auto list = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("list_mascots") },
        { QStringLiteral("selector"), QStringLiteral("mascot.anchor.x > 0") },
    }, service);
    expect(service.lastSelector == QStringLiteral("mascot.anchor.x > 0"),
        "dispatcher should pass list selector");
    expect(list.value(QStringLiteral("mascots")).toArray().size() == 1,
        "dispatcher should serialize listed mascots");

    auto spawn = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("spawn_mascot") },
        { QStringLiteral("request"), QJsonObject {
            { QStringLiteral("name"), QStringLiteral("Default") },
            { QStringLiteral("anchor"), QJsonObject {
                { QStringLiteral("x"), 5.0 },
                { QStringLiteral("y"), 6.0 },
            } },
        } },
    }, service);
    expect(spawn.value(QStringLiteral("mascot")).toObject()
        .value(QStringLiteral("anchor")).toObject()
        .value(QStringLiteral("x")).toDouble() == 5.0,
        "dispatcher should parse spawn request");

    auto codex = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("show_codex_notification") },
        { QStringLiteral("payload"), QJsonObject {
            { QStringLiteral("type"), QStringLiteral("agent-turn-complete") },
            { QStringLiteral("last-assistant-message"), QStringLiteral("done") },
        } },
    }, service);
    expect(codex.value(QStringLiteral("handled")).toBool(false),
        "dispatcher should route Codex completion notifications");

    auto newSession = MascotCommandDispatcher::dispatchRequest(QJsonObject {
        { QStringLiteral("command"), QStringLiteral("show_codex_notification") },
        { QStringLiteral("payload"), QJsonObject {
            { QStringLiteral("type"), QStringLiteral("session-title-updated") },
            { QStringLiteral("title"), QStringLiteral("New session") },
            { QStringLiteral("description"), QStringLiteral("Ready") },
        } },
    }, service);
    expect(newSession.value(QStringLiteral("handled")).toBool(false),
        "dispatcher should route Codex new-session title notifications");
}

void testSafeChildPath() {
    QTemporaryDir root;
    expect(root.isValid(), "safe path test root should be valid");
    expect(SafePath::safeChildPath(root.path(), QStringLiteral("img/frame.png")).has_value(),
        "safe child path should allow nested relative names");
    expect(!SafePath::safeChildPath(root.path(), QStringLiteral("../outside.png")).has_value(),
        "safe child path should reject parent traversal");
    expect(!SafePath::safeChildPath(root.path(), QStringLiteral("img/../../outside.png")).has_value(),
        "safe child path should reject embedded parent traversal");
    expect(!SafePath::safeChildPath(root.path(), QStringLiteral("C:\\outside.png")).has_value(),
        "safe child path should reject drive paths");
    expect(!SafePath::safeChildPath(root.path(), QStringLiteral("/etc/passwd")).has_value(),
        "safe child path should reject absolute paths");
}

void testScriptExecutionTimeout() {
    shijima::scripting::context context;
    auto started = std::chrono::steady_clock::now();
    bool result = context.eval_bool_with_timeout(
        "while (true) {}", std::chrono::milliseconds(25));
    auto elapsed = std::chrono::steady_clock::now() - started;
    expect(!result, "timed out script should evaluate to false");
    expect(elapsed < std::chrono::seconds(2),
        "script execution timeout should interrupt an infinite loop");
}

void testBroadcastTargetsNearbyMascots() {
    shijima::broadcast::manager broadcasts;
    auto far = broadcasts.start_broadcast("CuddleEvil", { 500, 100 });
    shijima::broadcast::client client;

    expect(!broadcasts.try_connect(client, { 0, 100 }, "CuddleEvil",
        "IHugYou", "IAmHugged"),
        "scan interactions should reject distant mascot targets");
    expect(!client.connected(),
        "a rejected distant target should not connect the client");
    expect(far.available(),
        "a rejected distant target should remain available");

    auto near = broadcasts.start_broadcast("CuddleEvil", { 200, 100 });
    auto nearest = broadcasts.start_broadcast("CuddleEvil", { 64, 100 });
    expect(broadcasts.try_connect(client, { 0, 100 }, "CuddleEvil",
        "IHugYou", "IAmHugged"),
        "scan interactions should connect to a nearby mascot target");
    expect(client.connected(),
        "an accepted nearby target should connect the client");
    expect(client.get_target().x == 64,
        "scan interactions should prefer the nearest available target");
    expect(near.available(),
        "a farther nearby target should remain available");
    expect(!nearest.available(),
        "the selected nearest target should be reserved");
}

void testBehaviorPreferenceRestoration() {
    using shijima::behavior::base;
    using shijima::behavior::list;

    auto hold = std::make_shared<base>(
        "Hold", 1, false, shijima::scripting::condition(true));
    auto natural = std::make_shared<base>(
        "Natural", 1, false, shijima::scripting::condition(true));
    auto other = std::make_shared<base>(
        "Other", 1, false, shijima::scripting::condition(true));
    hold->add_next = false;
    hold->next_list = std::make_unique<list>(
        std::vector<std::shared_ptr<base>> { natural });

    shijima::scripting::context script;
    shijima::behavior::manager behaviors(script,
        list(std::vector<std::shared_ptr<base>> { hold, natural, other }),
        "Hold");
    auto state = std::make_shared<shijima::mascot::state>();
    auto active = behaviors.next(state);
    expect(active == hold,
        "behavior preference fixture should select the hold behavior first");

    // Simulate the UI's temporary prefer_next_behavior() override, then clear
    // it as a release would.  The behavior's own custom next-list must win.
    behaviors.set_next("Other");
    behaviors.restore_next(active);
    auto restored = behaviors.next(state);
    expect(restored == natural,
        "clearing a temporary preference should restore Add=false next-list");
}

void testHotspotBehaviorRepeatsAcrossActionCompletion() {
    // Keep the fixture close to the installed Cerber package: a Stay action
    // with a 250ms pose, wrapped in a non-looping Sequence through an
    // ActionReference with Duration=3, plus a natural StandUp successor.
    // This exercises parser -> action -> mascot manager -> behavior manager,
    // rather than only testing next-list bookkeeping.
    std::string const actions = R"xml(
        <Mascot>
          <ActionList>
            <Action Name="Stand" Type="Stay">
              <Animation>
                <Pose Image="/stand.png" ImageAnchor="0,0" Velocity="0,0" Duration="250" />
                <Hotspot Shape="Rectangle" Origin="0,0" Size="10,10" Behavior="Pat" />
              </Animation>
            </Action>
            <Action Name="StandUp" Type="Sequence" Loop="false">
              <ActionReference Name="Stand" Duration="3" />
            </Action>
            <Action Name="PatAction" Type="Stay" Draggable="false">
              <Animation>
                <Pose Image="/pat.png" ImageAnchor="0,0" Velocity="0,0" Duration="250" />
                <Hotspot Shape="Rectangle" Origin="0,0" Size="10,10" Behavior="Pat" />
              </Animation>
            </Action>
            <Action Name="Pat" Type="Sequence" Loop="false">
              <ActionReference Name="PatAction" Duration="3" />
            </Action>
          </ActionList>
        </Mascot>
    )xml";
    std::string const behaviors = R"xml(
        <Mascot>
          <BehaviorList>
            <Behavior Name="Pat" Frequency="0" Hidden="true">
              <NextBehaviorList Add="false">
                <BehaviorReference Name="StandUp" Frequency="1" />
              </NextBehaviorList>
            </Behavior>
            <Behavior Name="StandUp" Frequency="1" />
          </BehaviorList>
        </Mascot>
    )xml";

    shijima::mascot::manager mascot(actions, behaviors,
        { { 0, 0 }, "StandUp", false });
    auto env = std::make_shared<shijima::mascot::environment>();
    env->floor = { 0, -100, 100 };
    env->ceiling = { -100, -100, 100 };
    env->screen = { -100, 100, 100, -100 };
    env->work_area = env->screen;
    env->subtick_count = 1;
    mascot.state->env = env;

    mascot.tick();
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "StandUp",
        "hotspot fixture should start in StandUp behavior");
    expect(mascot.hotspot_behavior({ 0, 0 }) == "Pat",
        "hotspot fixture should resolve Pat at the pressed coordinate");

    // A long press keeps this preference active once per action round.  The
    // first four ticks cross the real Cerber action boundary and must start
    // Pat again instead of falling back to StandUp.
    for (int i = 0; i < 4; ++i) {
        mascot.prefer_next_behavior("Pat");
        mascot.tick();
    }
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "Pat",
        "first completed pat action should restart Pat");

    // Keep the preference through another action boundary to verify that a
    // held gesture produces at least two complete pat rounds.
    for (int i = 0; i < 4; ++i) {
        mascot.prefer_next_behavior("Pat");
        mascot.tick();
    }
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "Pat",
        "second completed pat action should restart Pat while held");

    // Releasing restores the behavior's natural successor rather than
    // leaving the temporary preference in place.
    mascot.clear_preferred_next_behavior();
    for (int i = 0; i < 8 && mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "Pat"; ++i)
    {
        mascot.tick();
    }
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "StandUp",
        "releasing a held pat should restore the natural StandUp successor");
}

void testWindowPushBehaviorGate() {
    std::string const actions = R"xml(
        <Mascot>
          <ActionList>
            <Action Name="Idle" Type="Stay">
              <Animation>
                <Pose Image="/idle.png" ImageAnchor="0,0" Velocity="0,0" Duration="5" />
              </Animation>
            </Action>
            <Action Name="Fall" Type="Stay">
              <Animation>
                <Pose Image="/fall.png" ImageAnchor="0,0" Velocity="0,0" Duration="5" />
              </Animation>
            </Action>
            <Action Name="Throw" Type="Embedded"
                Class="com.group_finity.mascot.action.ThrowIE"
                BorderType="Floor" InitialVX="32" InitialVY="-10">
              <Animation>
                <Pose Image="/throw.png" ImageAnchor="0,0" Velocity="0,0" Duration="5" />
              </Animation>
            </Action>
          </ActionList>
        </Mascot>
    )xml";
    std::string const behaviors = R"xml(
        <Mascot>
          <BehaviorList>
            <Behavior Name="Throw" Frequency="1"
                Condition="#{mascot.environment.allowsWindowPushing &amp;&amp; mascot.environment.activeIE.visible}" />
            <Behavior Name="Idle" Frequency="1" />
            <Behavior Name="Fall" Frequency="0" />
          </BehaviorList>
        </Mascot>
    )xml";

    shijima::mascot::manager mascot(actions, behaviors,
        { { 0, 100 }, "", false });
    auto env = std::make_shared<shijima::mascot::environment>();
    env->floor = { 100, 0, 100 };
    env->ceiling = { 0, 0, 100 };
    env->screen = { 0, 100, 100, 0 };
    env->work_area = env->screen;
    env->active_ie = { 0, 100, 100, 0 };
    env->subtick_count = 1;
    int pushes = 0;
    env->window_push_callback = [&pushes](double dx, double dy) {
        ++pushes;
        return dx > 0 && dy == 0;
    };
    mascot.state->env = env;

    // The default policy is off, so a ThrowIE behavior must not even be
    // selected and no platform callback may run.
    mascot.tick();
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "Idle" && pushes == 0,
        "window pushing should be disabled by default at behavior selection");

    env->allows_window_pushing = true;
    expect(env->request_window_push(32, 0) && pushes == 1,
        "an enabled environment with an active window should request one push");

    env->allows_window_pushing = false;
    expect(!env->request_window_push(32, 0) && pushes == 1,
        "disabling window pushing should prevent subsequent platform requests");

    env->allows_window_pushing = true;
    env->active_ie = { -50, -50, -50, -50 };
    expect(!env->request_window_push(32, 0) && pushes == 1,
        "an unavailable active window should never receive a push request");
}

void testFallBoundaryPriorityOverActiveWindow() {
    // Keep the fixture close to the default mascot's Fall flow: a Sequence
    // wrapping the embedded Fall action, with a bounded successor after
    // landing. This exercises parser -> action -> manager -> behavior manager
    // against the Windows inclusive/exclusive bottom-edge mismatch.
    std::string const actions = R"xml(
        <Mascot>
          <ActionList>
            <Action Name="Fall" Type="Sequence" Loop="false">
              <ActionReference Name="Falling"/>
            </Action>
            <Action Name="Falling" Type="Embedded"
                Class="com.group_finity.mascot.action.Fall"
                RegistanceX="0.05" RegistanceY="0.1" Gravity="2">
              <Animation>
                <Pose Image="/fall.png" ImageAnchor="0,0" Velocity="0,0" Duration="250" />
              </Animation>
            </Action>
            <Action Name="Landed" Type="Stay" Duration="1000000">
              <Animation>
                <Pose Image="/stand.png" ImageAnchor="0,0" Velocity="0,0" Duration="250" />
              </Animation>
            </Action>
          </ActionList>
        </Mascot>
    )xml";
    std::string const behaviors = R"xml(
        <Mascot>
          <BehaviorList>
            <Behavior Name="Fall" Frequency="0" Hidden="true">
              <NextBehaviorList Add="false">
                <BehaviorReference Name="Landed" Frequency="1" />
              </NextBehaviorList>
            </Behavior>
            <Behavior Name="Landed" Frequency="1" />
          </BehaviorList>
        </Mascot>
    )xml";

    // A maximized window next to the taskbar exposes an exclusive Win32
    // bottom (1040) while the Qt floor is inclusive (1039). Dropping the
    // mascot below the floor must land it on the floor, not one pixel below.
    shijima::mascot::manager mascot(actions, behaviors,
        { { 960, 1050 }, "Fall", false });
    auto env = std::make_shared<shijima::mascot::environment>();
    env->screen = { 0, 1919, 1079, 0 };
    env->work_area = { 0, 1919, 1039, 0 };
    env->floor = { 1039, 0, 1919 };
    env->ceiling = { 0, 0, 1919 };
    env->active_ie = { 0, 1919, 1040, 0 };
    env->subtick_count = 1;
    mascot.state->env = env;

    mascot.tick();
    expect(mascot.state->anchor.y == 1039,
        "fall should clamp to the global floor when the active window "
        "bottom lies one pixel past it");
    expect(env->floor.is_on(mascot.state->anchor),
        "fall should be on the global floor after clamping");
    mascot.tick();
    expect(mascot.active_behavior() != nullptr &&
        mascot.active_behavior()->name == "Landed",
        "fall should complete into its successor after landing on the floor");
    expect(mascot.state->anchor.y == 1039,
        "a landed mascot should stay on the global floor");

    // An active window inside the screen must keep its normal top-edge
    // interaction: a falling mascot lands on the window top, not the floor.
    shijima::mascot::manager windowMascot(actions, behaviors,
        { { 960, 400 }, "Fall", false });
    auto windowEnv = std::make_shared<shijima::mascot::environment>();
    windowEnv->screen = { 0, 1919, 1079, 0 };
    windowEnv->work_area = { 0, 1919, 1039, 0 };
    windowEnv->floor = { 1039, 0, 1919 };
    windowEnv->ceiling = { 0, 0, 1919 };
    windowEnv->active_ie = { 500, 1919, 800, 0 };
    windowEnv->subtick_count = 1;
    windowMascot.state->env = windowEnv;

    int guard = 0;
    while (guard < 200) {
        windowMascot.tick();
        ++guard;
        auto behavior = windowMascot.active_behavior();
        if (behavior == nullptr || behavior->name != "Fall") {
            break;
        }
    }
    expect(guard < 200,
        "fall should reach an active window edge within bounded ticks");
    expect(windowMascot.state->anchor.y == 500,
        "fall should still stick to the active window top when no global "
        "boundary conflicts");
    expect(windowMascot.state->on_land(),
        "a mascot on the active window top should be on land");

    // Fall-through mode lowers the floor to the real screen bottom. The
    // inclusive Qt bottom (1079) must still win over the exclusive Win32
    // edge (1080) so long falls keep landing on the screen instead of
    // re-entering Fall below it. Starting below both edges exercises the
    // same clamp-then-IE-stick conflict as a mascot dragged off-screen.
    shijima::mascot::manager fallThrough(actions, behaviors,
        { { 960, 1090 }, "Fall", false });
    auto fallThroughEnv = std::make_shared<shijima::mascot::environment>();
    fallThroughEnv->screen = { 0, 1919, 1079, 0 };
    fallThroughEnv->work_area = { 0, 1919, 1079, 0 };
    fallThroughEnv->floor = { 1079, 0, 1919 };
    fallThroughEnv->ceiling = { 0, 0, 1919 };
    fallThroughEnv->active_ie = { 0, 1919, 1080, 0 };
    fallThroughEnv->subtick_count = 1;
    fallThrough.state->env = fallThroughEnv;

    guard = 0;
    while (guard < 200) {
        fallThrough.tick();
        ++guard;
        auto behavior = fallThrough.active_behavior();
        if (behavior == nullptr || behavior->name != "Fall") {
            break;
        }
    }
    expect(guard < 200,
        "fall-through should reach the screen bottom within bounded ticks");
    expect(fallThrough.state->anchor.y == 1079,
        "fall-through should land on the inclusive screen bottom instead of "
        "the exclusive Win32 edge");
    expect(fallThroughEnv->floor.is_on(fallThrough.state->anchor),
        "fall-through should end on the real screen-bottom floor");
}

void testMascotHoldGestureBoundaries() {
    using Gesture = shijima::ui::MascotHoldGesture;

    expect(!Gesture::reachesLongPress(Gesture::kTriggerAfterMs - 1, 0),
        "mascot hold should wait for the long-press threshold");
    expect(Gesture::reachesLongPress(Gesture::kTriggerAfterMs,
        Gesture::kMovementTolerancePx),
        "mascot hold should trigger at the threshold within tolerance");
    expect(!Gesture::reachesLongPress(Gesture::kTriggerAfterMs,
        Gesture::kMovementTolerancePx + 1),
        "mascot hold should cancel after crossing movement tolerance");
    expect(!Gesture::reachesLongPress(Gesture::kTriggerAfterMs, -1),
        "mascot hold should reject invalid movement distances");

    expect(Gesture::qualifiesAsClick(0,
        Gesture::kClickMovementTolerancePx),
        "a stationary press should remain a click candidate");
    expect(Gesture::qualifiesAsClick(Gesture::kClickDurationMs,
        Gesture::kClickMovementTolerancePx),
        "a click should include the duration boundary");
    expect(!Gesture::qualifiesAsClick(Gesture::kClickDurationMs + 1, 0),
        "a press beyond click duration should not become a click");
    expect(!Gesture::qualifiesAsClick(10,
        Gesture::kClickMovementTolerancePx + 1),
        "a moved press should not become a click");

    expect(!Gesture::cancelsForEvent(QEvent::FocusOut, Qt::LeftButton),
        "focus loss should not cancel a hold while left button remains down");
    expect(!Gesture::cancelsForEvent(QEvent::WindowDeactivate,
        Qt::LeftButton),
        "window deactivation should not cancel a held left-button gesture");
    expect(!Gesture::cancelsForEvent(QEvent::ApplicationDeactivate,
        Qt::LeftButton),
        "application deactivation should not cancel a held left-button gesture");
    expect(Gesture::cancelsForEvent(QEvent::FocusOut, Qt::NoButton),
        "focus loss after a lost release should cancel the stale gesture");
    expect(Gesture::cancelsForEvent(QEvent::UngrabMouse, Qt::LeftButton),
        "an explicit mouse ungrab should cancel even while left is down");
    expect(Gesture::cancelsForEvent(QEvent::Hide, Qt::LeftButton),
        "hiding a mascot should cancel its active gesture");
    expect(Gesture::cancelsForEvent(QEvent::Close, Qt::LeftButton),
        "closing a mascot should cancel its active gesture");
}

}

QByteArray testValidMascotInfoJson() {
    return QByteArrayLiteral(
        "{\"name\":\"Validate Me\",\"version\":\"1.2.3\","
        "\"description\":\"fixture\",\"author\":\"tester\"}");
}

QString testWriteValidMascotPackage(QString const& packagePath,
    QByteArray const& infoJson = QByteArray {})
{
    std::vector<TestZipEntry> entries {
        { QStringLiteral("info.json"),
            infoJson.isEmpty() ? testValidMascotInfoJson() : infoJson },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), minimalPngBytes() },
    };
    testWriteZip(packagePath, entries);
    return packagePath;
}

void testMascotPackageValidation() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");
    QDir tempDir(temp.path());

    QString validPath = testWriteValidMascotPackage(
        tempDir.absoluteFilePath(QStringLiteral("valid.mascot")));
    MascotPackageReport report;
    expect(MascotPackage::validatePackage(validPath, report),
        "validatePackage should accept a minimal valid package");
    expect(report.ok && report.errors.isEmpty(),
        "valid package report should be ok with no errors");
    expect(report.metadata.name == QStringLiteral("Validate Me") &&
        report.metadata.version == QStringLiteral("1.2.3") &&
        report.metadata.author == QStringLiteral("tester"),
        "valid package report should carry package metadata");
    expect(report.entryCount == 4 && report.fileCount == 4,
        "valid package report should report entry and file counts");
    expect(report.extractedBytes ==
        static_cast<std::uint64_t>(testValidMascotInfoJson().size() +
            minimalActionsXml().size() + minimalBehaviorsXml().size() +
            minimalPngBytes().size()),
        "valid package report should report extracted size");

    QString missingActionsPath = tempDir.absoluteFilePath(
        QStringLiteral("missing-actions.mascot"));
    testWriteZip(missingActionsPath, {
        { QStringLiteral("info.json"), testValidMascotInfoJson() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), minimalPngBytes() },
    });
    MascotPackageReport missingActions;
    expect(!MascotPackage::validatePackage(missingActionsPath, missingActions) &&
        !missingActions.ok,
        "validatePackage should reject a package without actions.xml");
    expect(missingActions.errors.join(QStringLiteral(";"))
        .contains(QStringLiteral("actions.xml")),
        "missing actions.xml should be listed in the error report");

    QString traversalPath = tempDir.absoluteFilePath(
        QStringLiteral("traversal.mascot"));
    std::vector<TestZipEntry> traversalEntries {
        { QStringLiteral("info.json"), testValidMascotInfoJson() },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("../evil.png"), minimalPngBytes() },
    };
    testWriteZip(traversalPath, traversalEntries);
    MascotPackageReport traversal;
    expect(!MascotPackage::validatePackage(traversalPath, traversal),
        "validatePackage should reject path traversal entries");
    expect(traversal.errors.join(QStringLiteral(";"))
        .contains(QStringLiteral("unsafe")),
        "path traversal should be reported as unsafe");

    QString payloadPath = tempDir.absoluteFilePath(
        QStringLiteral("payload.mascot"));
    std::vector<TestZipEntry> payloadEntries {
        { QStringLiteral("info.json"), testValidMascotInfoJson() },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("sound/evil.exe"), QByteArrayLiteral("MZ") },
    };
    testWriteZip(payloadPath, payloadEntries);
    MascotPackageReport payload;
    expect(!MascotPackage::validatePackage(payloadPath, payload),
        "validatePackage should reject executable payloads in sound/");
    expect(payload.errors.join(QStringLiteral(";"))
        .contains(QStringLiteral("forbidden")),
        "executable payloads should be reported as forbidden");

    QString nestedPath = tempDir.absoluteFilePath(
        QStringLiteral("nested.mascot"));
    std::vector<TestZipEntry> nestedEntries {
        { QStringLiteral("info.json"), testValidMascotInfoJson() },
        { QStringLiteral("actions.xml"), minimalActionsXml() },
        { QStringLiteral("behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("img/evil.zip"), QByteArrayLiteral("PK") },
    };
    testWriteZip(nestedPath, nestedEntries);
    MascotPackageReport nested;
    expect(!MascotPackage::validatePackage(nestedPath, nested),
        "validatePackage should reject nested archives");
}

void testCliMascotValidateParsing() {
    char argv0[] = "NeurolingsCE-cli";
    char mascotOption[] = "--mascot";
    char jsonOption[] = "--json";
    char action[] = "validate";
    char packagePath[] = "C:/tmp/fixture.mascot";
    char *validArgv[] = { argv0, jsonOption, mascotOption, action, packagePath };
    auto valid = parseCliArguments(5, validArgv);
    expect(!valid.hasError && valid.hasCommand &&
        valid.command.kind == CliCommandKind::DocumentMascot &&
        valid.command.mascotAction == QStringLiteral("validate") &&
        valid.command.mascotArchivePath == QStringLiteral("C:/tmp/fixture.mascot") &&
        valid.global.json,
        "CLI should parse --mascot validate FILE with --json");

    char *missingPathArgv[] = { argv0, mascotOption, action };
    auto missing = parseCliArguments(3, missingPathArgv);
    expect(missing.hasError &&
        missing.error.error.contains(QStringLiteral("Missing mascot package path")) &&
        missing.error.exitCode == 2,
        "CLI should require a package path for --mascot validate");

    char extra[] = "extra";
    char *extraArgv[] = { argv0, mascotOption, action, packagePath, extra };
    auto extraArgs = parseCliArguments(5, extraArgv);
    expect(extraArgs.hasError &&
        extraArgs.error.error.contains(QStringLiteral("Unexpected argument")),
        "CLI should reject extra arguments after --mascot validate FILE");

    char unknownAction[] = "frobnicate";
    char *unknownArgv[] = { argv0, mascotOption, unknownAction };
    auto unknown = parseCliArguments(3, unknownArgv);
    expect(unknown.hasError &&
        unknown.error.error.contains(QStringLiteral("list, add, remove, or validate")),
        "CLI should enumerate supported mascot actions on unknown actions");

    char listAction[] = "list";
    char *listArgv[] = { argv0, jsonOption, mascotOption, listAction };
    auto list = parseCliArguments(4, listArgv);
    expect(!list.hasError && list.command.mascotAction == QStringLiteral("list"),
        "existing --mascot list parsing should remain intact");
}

int main() {
    testMascotPatchParsing();
    testJsonRoundTrips();
    testStatusJson();
    testCodexActivityParsing();
    testCodexCliParsing();
    testCodexConfigManagement();
    testMascotPackageNames();
    testLegacyArchiveAnalysisAndConversion();
    testImportArchiveSupportsLegacyTemplateDirectory();
    testMascotPackageValidation();
    testCliMascotValidateParsing();
    testPackageInspectionRejectsOversizedPngHeader();
    testPackageInspectionRejectsMalformedPng();
    testCommandDispatcher();
    testSafeChildPath();
    testScriptExecutionTimeout();
    testBroadcastTargetsNearbyMascots();
    testBehaviorPreferenceRestoration();
    testHotspotBehaviorRepeatsAcrossActionCompletion();
    testWindowPushBehaviorGate();
    testFallBoundaryPriorityOverActiveWindow();
    testMascotHoldGestureBoundaries();

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "All app core tests passed" << std::endl;
    return EXIT_SUCCESS;
}
