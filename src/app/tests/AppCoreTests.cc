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
#include "shijima-qt/MascotApi.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/SafePath.hpp"
#include "core/shijima-engine/shijima/scripting/context.hpp"

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
}

void testLegacyArchiveAnalysisAndConversion() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary directory should be available");
    QDir tempDir(temp.path());
    QString archivePath = tempDir.absoluteFilePath(QStringLiteral("legacy.zip"));
    QString outputPath = tempDir.absoluteFilePath(QStringLiteral("out"));
    QDir().mkpath(outputPath);

    MascotMetadata alphaMetadata;
    alphaMetadata.name = QStringLiteral("Alpha");
    alphaMetadata.version = QStringLiteral("1.0");
    alphaMetadata.author = QStringLiteral("tester");

    std::vector<TestZipEntry> entries {
        { QStringLiteral("Alpha/actions.xml"), minimalActionsXml() },
        { QStringLiteral("Alpha/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Alpha/img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("Alpha/info.json"), MascotPackage::metadataToJson(alphaMetadata) },
        { QStringLiteral("Beta/actions.xml"), minimalActionsXml() },
        { QStringLiteral("Beta/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Beta/img/shime1.png"), minimalPngBytes() },
        { QStringLiteral("Broken/behaviors.xml"), minimalBehaviorsXml() },
        { QStringLiteral("Broken/img/shime1.png"), minimalPngBytes() },
    };
    testWriteZip(archivePath, entries);

    auto analysis = MascotPackage::analyzeLegacyArchive(archivePath);
    expect(analysis.ok, "legacy archive with valid candidates should be OK");
    expect(analysis.candidates.size() >= 3,
        "analysis should include valid and incomplete candidates");

    auto findCandidate = [&analysis](QString const& name) {
        auto it = std::find_if(analysis.candidates.begin(), analysis.candidates.end(),
            [&name](LegacyMascotCandidate const& candidate) {
                return candidate.name == name || candidate.metadata.name == name;
            });
        return it == analysis.candidates.end() ? nullptr : &(*it);
    };

    auto *alpha = findCandidate(QStringLiteral("Alpha"));
    expect(alpha != nullptr && alpha->convertible,
        "valid candidate with metadata should be convertible");
    auto *beta = findCandidate(QStringLiteral("Beta"));
    expect(beta != nullptr && beta->convertible && beta->generatedMetadata,
        "candidate without info.json should use fallback metadata");
    auto *broken = findCandidate(QStringLiteral("Broken"));
    expect(broken != nullptr && !broken->convertible &&
        broken->errors.join(QStringLiteral(";")).contains(QStringLiteral("actions.xml")),
        "candidate missing actions.xml should report a content error");

    QFile existing(QDir(outputPath).absoluteFilePath(QStringLiteral("Alpha.mascot")));
    expect(existing.open(QFile::WriteOnly), "existing package placeholder should be writable");
    existing.write("existing");
    existing.close();

    auto results = MascotPackage::writeLegacyArchiveSelectionAsPackages(
        archivePath, outputPath, QStringList {
            QStringLiteral("Alpha"),
            QStringLiteral("Beta"),
        });
    expect(results.size() == 2, "selected legacy candidates should produce two results");
    expect(std::all_of(results.begin(), results.end(),
        [](LegacyMascotConversionResult const& result) { return result.ok; }),
        "selected valid candidates should convert successfully");

    bool alphaAvoidedOverwrite = std::any_of(results.begin(), results.end(),
        [](LegacyMascotConversionResult const& result) {
            return result.name == QStringLiteral("Alpha") &&
                result.packagePath.endsWith(QStringLiteral("Alpha-2.mascot"));
        });
    expect(alphaAvoidedOverwrite, "conversion should avoid overwriting existing packages");

    for (auto const& result : results) {
        MascotMetadata metadata;
        QString error;
        expect(MascotPackage::inspectPackage(result.packagePath, metadata, error),
            "converted package should pass package inspection");
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

}

int main() {
    testMascotPatchParsing();
    testJsonRoundTrips();
    testStatusJson();
    testMascotPackageNames();
    testLegacyArchiveAnalysisAndConversion();
    testPackageInspectionRejectsOversizedPngHeader();
    testCommandDispatcher();
    testSafeChildPath();
    testScriptExecutionTimeout();

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "All app core tests passed" << std::endl;
    return EXIT_SUCCESS;
}
