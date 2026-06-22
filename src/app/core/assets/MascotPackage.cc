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

#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/SafePath.hpp"

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <shimejifinder/analyze.hpp>
#include <shimejifinder/extractor.hpp>
#include <shimejifinder/libunarr/archive.hpp>
#include <shimejifinder/memory_extractor.hpp>

namespace {

class ExactPathExtractor : public shimejifinder::extractor {
public:
    explicit ExactPathExtractor(QString root): m_root(QDir::cleanPath(root)) {}

    void begin_write(shimejifinder::extract_target const& target) override {
        QString relative = QString::fromStdString(target.extract_name());
        auto safePath = SafePath::safeChildPath(m_root, relative);
        if (!safePath.has_value()) {
            throw std::runtime_error("Unsafe package extraction path");
        }
        QString filePath = safePath.value();
        std::filesystem::path path = filePath.toStdString();
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out;
        out.open(path, std::ios::out | std::ios::binary);
        if (!out.is_open()) {
            throw std::runtime_error("Could not open package extraction target");
        }
        m_activeWrites.emplace_back(std::move(out));
    }

    void write_next(size_t offset, const void *buf, size_t size) override {
        for (auto &stream : m_activeWrites) {
            stream.seekp(offset);
            stream.write(static_cast<const char *>(buf), size);
        }
    }

    void end_write() override {
        for (auto &stream : m_activeWrites) {
            stream.close();
        }
        m_activeWrites.clear();
    }

    void finalize() override {
        m_activeWrites.clear();
    }

private:
    QString m_root;
    std::vector<std::ofstream> m_activeWrites;
};

QString normalizedArchivePath(QString path) {
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList cleanParts;
    for (auto const& part : parts) {
        if (part == QStringLiteral(".") || part == QStringLiteral("..") ||
            part.contains(QLatin1Char(':')))
        {
            return {};
        }
        cleanParts.append(part);
    }
    return cleanParts.join(QLatin1Char('/'));
}

bool isSupportedPackagePath(QString const& path) {
    QString lower = path.toLower();
    return lower == QStringLiteral("info.json") ||
        lower == QStringLiteral("bubble_context.txt") ||
        lower == QStringLiteral("actions.xml") ||
        lower == QStringLiteral("behaviors.xml") ||
        (lower.startsWith(QStringLiteral("img/")) &&
            lower.endsWith(QStringLiteral(".png"))) ||
        lower.startsWith(QStringLiteral("sound/"));
}

bool openPackage(QString const& packagePath, shimejifinder::libunarr::archive &archive,
    QString &errorMessage)
{
    try {
        archive.open(packagePath.toStdString());
        return true;
    }
    catch (std::exception const& ex) {
        errorMessage = QString::fromUtf8(ex.what());
        return false;
    }
}

QByteArray readPackageFile(QString const& packagePath, QString const& wantedPath,
    QString &errorMessage)
{
    shimejifinder::libunarr::archive archive;
    if (!openPackage(packagePath, archive, errorMessage)) {
        return {};
    }

    QString normalizedWanted = normalizedArchivePath(wantedPath).toLower();
    bool found = false;
    for (size_t i = 0; i < archive.size(); ++i) {
        auto entry = archive.at(i);
        QString entryPath = normalizedArchivePath(QString::fromStdString(entry->path()));
        if (entryPath.toLower() == normalizedWanted) {
            entry->add_target(shimejifinder::extract_target(
                normalizedWanted.toStdString()));
            found = true;
            break;
        }
    }
    if (!found) {
        errorMessage = QStringLiteral("Package is missing %1").arg(wantedPath);
        return {};
    }

    shimejifinder::memory_extractor extractor;
    try {
        static_cast<shimejifinder::archive&>(archive).extract(&extractor);
    }
    catch (std::exception const& ex) {
        errorMessage = QString::fromUtf8(ex.what());
        return {};
    }

    auto key = normalizedWanted.toStdString();
    if (!extractor.contains(key)) {
        errorMessage = QStringLiteral("Could not read %1").arg(wantedPath);
        return {};
    }
    auto const& data = extractor.data(key);
    return QByteArray(data.data(), (qsizetype)data.size());
}

bool inspectEntries(QString const& packagePath, MascotMetadata &metadata,
    QString &errorMessage)
{
    QByteArray infoJson = readPackageFile(packagePath, QStringLiteral("info.json"),
        errorMessage);
    if (infoJson.isEmpty()) {
        return false;
    }
    try {
        metadata = MascotPackage::metadataFromJson(infoJson);
    }
    catch (std::exception const& ex) {
        errorMessage = QString::fromUtf8(ex.what());
        return false;
    }

    shimejifinder::libunarr::archive archive;
    if (!openPackage(packagePath, archive, errorMessage)) {
        return false;
    }
    bool hasActions = false;
    bool hasBehaviors = false;
    bool hasImage = false;
    for (size_t i = 0; i < archive.size(); ++i) {
        QString path = normalizedArchivePath(QString::fromStdString(archive.at(i)->path()));
        QString lower = path.toLower();
        hasActions = hasActions || lower == QStringLiteral("actions.xml");
        hasBehaviors = hasBehaviors || lower == QStringLiteral("behaviors.xml");
        hasImage = hasImage || (lower.startsWith(QStringLiteral("img/")) &&
            lower.endsWith(QStringLiteral(".png")));
    }
    if (!hasActions || !hasBehaviors || !hasImage) {
        errorMessage = QStringLiteral("Package must contain actions.xml, behaviors.xml, and img/*.png");
        return false;
    }
    return true;
}

quint32 crc32(QByteArray const& bytes) {
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

void write16(QFile &file, quint16 value) {
    char data[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
    };
    file.write(data, 2);
}

void write32(QFile &file, quint32 value) {
    char data[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    file.write(data, 4);
}

struct ZipEntry {
    QString name;
    QByteArray data;
    quint32 crc = 0;
    quint32 offset = 0;
};

struct RawLegacyCandidateFlags {
    bool hasActions = false;
    bool hasBehaviors = false;
    bool hasImage = false;
};

QString normalizedLegacyCandidateName(QString name)
{
    if (name.endsWith(QStringLiteral(".mascot"), Qt::CaseInsensitive)) {
        name.chop(7);
    }
    return name;
}

bool addFileEntry(QString const& root, QString const& filePath,
    std::vector<ZipEntry> &entries, QString &errorMessage)
{
    QString relative = QDir(root).relativeFilePath(filePath).replace(
        QLatin1Char('\\'), QLatin1Char('/'));
    relative = normalizedArchivePath(relative);
    if (relative.isEmpty() || !isSupportedPackagePath(relative)) {
        return true;
    }

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        errorMessage = QStringLiteral("Could not read %1").arg(filePath);
        return false;
    }

    ZipEntry entry;
    entry.name = relative;
    entry.data = file.readAll();
    entry.crc = crc32(entry.data);
    entries.push_back(entry);
    return true;
}

QMap<QString, RawLegacyCandidateFlags> rawLegacyCandidates(QString const& archivePath)
{
    QMap<QString, RawLegacyCandidateFlags> candidates;
    shimejifinder::libunarr::archive archive;
    QString error;
    if (!openPackage(archivePath, archive, error)) {
        return candidates;
    }

    QFileInfo archiveInfo(archivePath);
    for (size_t i = 0; i < archive.size(); ++i) {
        QString path = normalizedArchivePath(QString::fromStdString(archive.at(i)->path()));
        QString lower = path.toLower();
        QString root;
        if (lower.endsWith(QStringLiteral("/actions.xml")) ||
            lower.endsWith(QStringLiteral("/action.xml")) ||
            lower.endsWith(QStringLiteral("/one.xml")) ||
            lower.endsWith(QString::fromUtf8("/\xe5\x8b\x95\xe4\xbd\x9c.xml")))
        {
            root = path.left(path.lastIndexOf(QLatin1Char('/')));
            root = normalizedLegacyCandidateName(root);
            candidates[root].hasActions = true;
        }
        else if (lower == QStringLiteral("actions.xml") ||
            lower == QStringLiteral("action.xml") ||
            lower == QStringLiteral("one.xml") ||
            lower == QString::fromUtf8("\xe5\x8b\x95\xe4\xbd\x9c.xml"))
        {
            root = archiveInfo.completeBaseName();
            candidates[root].hasActions = true;
        }
        else if (lower.endsWith(QStringLiteral("/behaviors.xml")) ||
            lower.endsWith(QStringLiteral("/behavior.xml")) ||
            lower.endsWith(QStringLiteral("/two.xml")) ||
            lower.endsWith(QString::fromUtf8("/\xe8\xa1\x8c\xe5\x8b\x95.xml")))
        {
            root = path.left(path.lastIndexOf(QLatin1Char('/')));
            root = normalizedLegacyCandidateName(root);
            candidates[root].hasBehaviors = true;
        }
        else if (lower == QStringLiteral("behaviors.xml") ||
            lower == QStringLiteral("behavior.xml") ||
            lower == QStringLiteral("two.xml") ||
            lower == QString::fromUtf8("\xe8\xa1\x8c\xe5\x8b\x95.xml"))
        {
            root = archiveInfo.completeBaseName();
            candidates[root].hasBehaviors = true;
        }
        else {
            qsizetype imgIndex = lower.indexOf(QStringLiteral("/img/"));
            if (imgIndex >= 0 && lower.endsWith(QStringLiteral(".png"))) {
                root = path.left(imgIndex);
                root = normalizedLegacyCandidateName(root);
                candidates[root].hasImage = true;
            }
            else if (lower.startsWith(QStringLiteral("img/")) &&
                lower.endsWith(QStringLiteral(".png")))
            {
                root = archiveInfo.completeBaseName();
                candidates[root].hasImage = true;
            }
        }
    }
    return candidates;
}

void writeLocalEntry(QFile &file, ZipEntry &entry) {
    QByteArray name = entry.name.toUtf8();
    entry.offset = static_cast<quint32>(file.pos());
    write32(file, 0x04034b50);
    write16(file, 20);
    write16(file, 0x0800);
    write16(file, 0);
    write16(file, 0);
    write16(file, 0);
    write32(file, entry.crc);
    write32(file, static_cast<quint32>(entry.data.size()));
    write32(file, static_cast<quint32>(entry.data.size()));
    write16(file, static_cast<quint16>(name.size()));
    write16(file, 0);
    file.write(name);
    file.write(entry.data);
}

void writeCentralEntry(QFile &file, ZipEntry const& entry) {
    QByteArray name = entry.name.toUtf8();
    write32(file, 0x02014b50);
    write16(file, 20);
    write16(file, 20);
    write16(file, 0x0800);
    write16(file, 0);
    write16(file, 0);
    write16(file, 0);
    write32(file, entry.crc);
    write32(file, static_cast<quint32>(entry.data.size()));
    write32(file, static_cast<quint32>(entry.data.size()));
    write16(file, static_cast<quint16>(name.size()));
    write16(file, 0);
    write16(file, 0);
    write16(file, 0);
    write16(file, 0);
    write32(file, 0);
    write32(file, entry.offset);
    file.write(name);
}

void ensureLegacyMetadata(QString const& sourcePath, QString const& fallbackName) {
    QFile info(QDir(sourcePath).absoluteFilePath(QStringLiteral("info.json")));
    if (info.exists()) {
        return;
    }
    MascotMetadata metadata;
    metadata.name = fallbackName;
    QSaveFile out(info.fileName());
    if (out.open(QFile::WriteOnly)) {
        out.write(MascotPackage::metadataToJson(metadata));
        out.commit();
    }
}

void writeFallbackMetadata(QString const& sourcePath, QString const& fallbackName)
{
    MascotMetadata metadata;
    metadata.name = fallbackName;
    QSaveFile out(QDir(sourcePath).absoluteFilePath(QStringLiteral("info.json")));
    if (out.open(QFile::WriteOnly)) {
        out.write(MascotPackage::metadataToJson(metadata));
        out.commit();
    }
}

void tryExtractBubbleContext(QString const& archivePath, QString const& mascotName,
    QString const& targetPath)
{
    if (QFile::exists(QDir(targetPath).absoluteFilePath(QStringLiteral("bubble_context.txt")))) {
        return;
    }

    shimejifinder::libunarr::archive archive;
    QString error;
    if (!openPackage(archivePath, archive, error)) {
        return;
    }

    QStringList matches;
    QString mascotLower = mascotName.toLower();
    for (size_t i = 0; i < archive.size(); ++i) {
        QString path = normalizedArchivePath(QString::fromStdString(archive.at(i)->path()));
        QString lower = path.toLower();
        if (!lower.endsWith(QStringLiteral("bubble_context.txt"))) {
            continue;
        }
        if (lower == QStringLiteral("bubble_context.txt") ||
            lower.contains(QStringLiteral("/") + mascotLower + QStringLiteral("/")) ||
            lower.startsWith(mascotLower + QStringLiteral("/")) ||
            lower.startsWith(mascotLower + QStringLiteral(".mascot/")))
        {
            matches.append(path);
        }
    }
    if (matches.isEmpty()) {
        return;
    }

    QByteArray bytes = readPackageFile(archivePath, matches.first(), error);
    if (bytes.isEmpty()) {
        return;
    }
    QSaveFile out(QDir(targetPath).absoluteFilePath(QStringLiteral("bubble_context.txt")));
    if (out.open(QFile::WriteOnly)) {
        out.write(bytes);
        out.commit();
    }
}

LegacyMascotCandidate inspectLegacyDirectory(QString const& sourcePath,
    QString const& fallbackName)
{
    LegacyMascotCandidate candidate;
    candidate.name = fallbackName;
    candidate.metadata.name = fallbackName;

    QDir sourceDir(sourcePath);
    QFileInfo actions(sourceDir.absoluteFilePath(QStringLiteral("actions.xml")));
    QFileInfo behaviors(sourceDir.absoluteFilePath(QStringLiteral("behaviors.xml")));
    QDir imgDir(sourceDir.absoluteFilePath(QStringLiteral("img")));

    if (!actions.exists() || !actions.isFile()) {
        candidate.errors.append(QStringLiteral("Missing actions.xml"));
    }
    if (!behaviors.exists() || !behaviors.isFile()) {
        candidate.errors.append(QStringLiteral("Missing behaviors.xml"));
    }
    QStringList images = imgDir.entryList(QStringList { QStringLiteral("*.png") },
        QDir::Files);
    if (images.isEmpty()) {
        candidate.errors.append(QStringLiteral("Missing img/*.png"));
    }

    QFile info(sourceDir.absoluteFilePath(QStringLiteral("info.json")));
    if (info.exists() && info.open(QFile::ReadOnly)) {
        try {
            candidate.metadata = MascotPackage::metadataFromJson(info.readAll());
            candidate.name = candidate.metadata.name;
        }
        catch (std::exception const& ex) {
            candidate.generatedMetadata = true;
            candidate.warnings.append(QStringLiteral(
                "info.json is invalid; fallback metadata will be generated (%1)")
                .arg(QString::fromUtf8(ex.what())));
        }
    }
    else {
        candidate.generatedMetadata = true;
        candidate.warnings.append(QStringLiteral(
            "Missing info.json; fallback metadata will be generated"));
    }

    if (candidate.metadata.name.trimmed().isEmpty()) {
        candidate.metadata.name = fallbackName;
        candidate.name = fallbackName;
    }
    candidate.convertible = candidate.errors.isEmpty();
    return candidate;
}

QString uniquePackagePath(QString const& outputPath, QString const& name,
    QSet<QString> &reserved)
{
    QString base = MascotPackage::sanitizedPackageBaseName(name);
    QDir outputDir(outputPath);
    QString candidate = outputDir.absoluteFilePath(base + QStringLiteral(".mascot"));
    int suffix = 2;
    while (QFileInfo::exists(candidate) || reserved.contains(candidate)) {
        candidate = outputDir.absoluteFilePath(
            QStringLiteral("%1-%2.mascot").arg(base).arg(suffix++));
    }
    reserved.insert(candidate);
    return candidate;
}

}

namespace MascotPackage {

MascotMetadata defaultMetadata() {
    MascotMetadata metadata;
    metadata.name = QStringLiteral("Default");
    metadata.version = QStringLiteral("1.0");
    metadata.description = QStringLiteral("Default mascot for the application.");
    metadata.author = QStringLiteral("pixelomer[https://github.com/pixelomer]");
    return metadata;
}

MascotMetadata metadataFromJson(QByteArray const& bytes) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        throw std::runtime_error("Invalid info.json");
    }
    QJsonObject object = doc.object();
    MascotMetadata metadata;
    metadata.name = object.value(QStringLiteral("name")).toString().trimmed();
    metadata.version = object.value(QStringLiteral("version")).toString();
    metadata.description = object.value(QStringLiteral("description")).toString();
    metadata.author = object.value(QStringLiteral("author")).toString();
    if (metadata.name.isEmpty()) {
        throw std::runtime_error("info.json must contain a non-empty name");
    }
    return metadata;
}

QByteArray metadataToJson(MascotMetadata const& metadata) {
    QJsonObject object;
    object["name"] = metadata.name;
    object["version"] = metadata.version;
    object["description"] = metadata.description;
    object["author"] = metadata.author;
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

QString sanitizedPackageBaseName(QString const& name) {
    QString result = name.trimmed();
    static QString const invalid = QStringLiteral("<>:\"/\\|?*");
    for (auto ch : invalid) {
        result.replace(ch, QLatin1Char('_'));
    }
    for (qsizetype i = 0; i < result.size(); ++i) {
        if (result[i].unicode() < 32) {
            result[i] = QLatin1Char('_');
        }
    }
    result = result.trimmed();
    while (result.endsWith(QLatin1Char('.'))) {
        result.chop(1);
    }
    if (result.isEmpty()) {
        result = QStringLiteral("Mascot");
    }
    return result;
}

QString packagePathForName(QString const& storagePath, QString const& name) {
    return QDir(storagePath).absoluteFilePath(
        sanitizedPackageBaseName(name) + QStringLiteral(".mascot"));
}

QString cachePathForName(QString const& cacheRootPath, QString const& name) {
    return QDir(cacheRootPath).absoluteFilePath(sanitizedPackageBaseName(name));
}

bool inspectPackage(QString const& packagePath, MascotMetadata &metadata,
    QString &errorMessage)
{
    bool ok = inspectEntries(packagePath, metadata, errorMessage);
    if (ok) {
        APP_LOG_DEBUG("package") << "Inspected mascot package path=\""
            << packagePath.toStdString() << "\" name=\""
            << metadata.name.toStdString() << "\"";
    }
    else {
        APP_LOG_WARN("package") << "Invalid mascot package path=\""
            << packagePath.toStdString() << "\": "
            << errorMessage.toStdString();
    }
    return ok;
}

bool extractPackage(QString const& packagePath, QString const& outputPath,
    QString &errorMessage)
{
    APP_LOG_INFO("package") << "Extracting mascot package path=\""
        << packagePath.toStdString() << "\" output=\""
        << outputPath.toStdString() << "\"";
    shimejifinder::libunarr::archive archive;
    if (!openPackage(packagePath, archive, errorMessage)) {
        return false;
    }

    QDir outputDir(outputPath);
    if (outputDir.exists()) {
        outputDir.removeRecursively();
    }
    outputDir.mkpath(QStringLiteral("."));

    bool added = false;
    for (size_t i = 0; i < archive.size(); ++i) {
        auto entry = archive.at(i);
        QString path = normalizedArchivePath(QString::fromStdString(entry->path()));
        if (path.isEmpty() || !isSupportedPackagePath(path)) {
            continue;
        }
        entry->add_target(shimejifinder::extract_target(path.toStdString()));
        added = true;
    }
    if (!added) {
        errorMessage = QStringLiteral("Package does not contain any supported files");
        APP_LOG_WARN("package") << "Mascot package has no supported files path=\""
            << packagePath.toStdString() << "\"";
        return false;
    }

    ExactPathExtractor extractor(outputPath);
    try {
        static_cast<shimejifinder::archive&>(archive).extract(&extractor);
        APP_LOG_INFO("package") << "Extracted mascot package path=\""
            << packagePath.toStdString() << "\"";
        return true;
    }
    catch (std::exception const& ex) {
        errorMessage = QString::fromUtf8(ex.what());
        APP_LOG_ERROR("package") << "Failed to extract mascot package path=\""
            << packagePath.toStdString() << "\": " << ex.what();
        return false;
    }
}

bool writePackageFromDirectory(QString const& sourcePath,
    QString const& packagePath, QString &errorMessage)
{
    APP_LOG_INFO("package") << "Writing mascot package source=\""
        << sourcePath.toStdString() << "\" target=\""
        << packagePath.toStdString() << "\"";
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isDir()) {
        errorMessage = QStringLiteral("Source mascot directory does not exist");
        return false;
    }

    std::vector<ZipEntry> entries;
    QDirIterator iter(sourceInfo.absoluteFilePath(), QDir::Files,
        QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        if (!addFileEntry(sourceInfo.absoluteFilePath(), iter.next(), entries,
            errorMessage))
        {
            return false;
        }
    }
    std::sort(entries.begin(), entries.end(), [](ZipEntry const& lhs,
        ZipEntry const& rhs) {
        return lhs.name < rhs.name;
    });

    bool hasInfo = false;
    bool hasActions = false;
    bool hasBehaviors = false;
    bool hasImage = false;
    for (auto const& entry : entries) {
        QString lower = entry.name.toLower();
        hasInfo = hasInfo || lower == QStringLiteral("info.json");
        hasActions = hasActions || lower == QStringLiteral("actions.xml");
        hasBehaviors = hasBehaviors || lower == QStringLiteral("behaviors.xml");
        hasImage = hasImage || lower.startsWith(QStringLiteral("img/"));
    }
    if (!hasInfo || !hasActions || !hasBehaviors || !hasImage) {
        errorMessage = QStringLiteral("Mascot package source is missing required files");
        return false;
    }

    QFileInfo packageInfo(packagePath);
    QDir().mkpath(packageInfo.absolutePath());
    QFile file(packagePath);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        errorMessage = QStringLiteral("Could not write %1").arg(packagePath);
        APP_LOG_ERROR("package") << "Could not open mascot package for writing path=\""
            << packagePath.toStdString() << "\"";
        return false;
    }

    for (auto &entry : entries) {
        writeLocalEntry(file, entry);
    }
    quint32 centralOffset = static_cast<quint32>(file.pos());
    for (auto const& entry : entries) {
        writeCentralEntry(file, entry);
    }
    quint32 centralSize = static_cast<quint32>(file.pos()) - centralOffset;
    write32(file, 0x06054b50);
    write16(file, 0);
    write16(file, 0);
    write16(file, static_cast<quint16>(entries.size()));
    write16(file, static_cast<quint16>(entries.size()));
    write32(file, centralSize);
    write32(file, centralOffset);
    write16(file, 0);
    APP_LOG_INFO("package") << "Wrote mascot package target=\""
        << packagePath.toStdString() << "\" entries=" << entries.size();
    return true;
}

bool installPackage(QString const& packagePath, QString const& storagePath,
    QString &installedName, QString &errorMessage)
{
    APP_LOG_INFO("package") << "Installing mascot package path=\""
        << packagePath.toStdString() << "\" storage=\""
        << storagePath.toStdString() << "\"";
    MascotMetadata metadata;
    if (!inspectPackage(packagePath, metadata, errorMessage)) {
        return false;
    }
    QDir storageDir(storagePath);
    storageDir.mkpath(QStringLiteral("."));
    QString targetPath = packagePathForName(storagePath, metadata.name);
    QFileInfo sourceInfo(packagePath);
    QFileInfo targetInfo(targetPath);
    if (sourceInfo.absoluteFilePath() != targetInfo.absoluteFilePath()) {
        QFile::remove(targetPath);
        if (!QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
            errorMessage = QStringLiteral("Could not copy package into mascot storage");
            APP_LOG_ERROR("package") << "Could not copy mascot package source=\""
                << sourceInfo.absoluteFilePath().toStdString()
                << "\" target=\"" << targetPath.toStdString() << "\"";
            return false;
        }
    }
    installedName = metadata.name;
    APP_LOG_INFO("package") << "Installed mascot package name=\""
        << installedName.toStdString() << "\" target=\""
        << targetPath.toStdString() << "\"";
    return true;
}

bool packageLegacyDirectory(QString const& sourcePath,
    QString const& storagePath, QString const& fallbackName,
    QString &installedName, QString &errorMessage)
{
    ensureLegacyMetadata(sourcePath, fallbackName);
    QFile infoFile(QDir(sourcePath).absoluteFilePath(QStringLiteral("info.json")));
    if (!infoFile.open(QFile::ReadOnly)) {
        errorMessage = QStringLiteral("Could not read generated info.json");
        return false;
    }
    MascotMetadata metadata;
    try {
        metadata = metadataFromJson(infoFile.readAll());
    }
    catch (...) {
        metadata.name = fallbackName;
        QSaveFile out(infoFile.fileName());
        if (out.open(QFile::WriteOnly)) {
            out.write(metadataToJson(metadata));
            out.commit();
        }
    }
    QString targetPath = packagePathForName(storagePath, metadata.name);
    QFile::remove(targetPath);
    if (!writePackageFromDirectory(sourcePath, targetPath, errorMessage)) {
        return false;
    }
    installedName = metadata.name;
    return true;
}

LegacyArchiveAnalysis analyzeLegacyArchive(QString const& archivePath)
{
    APP_LOG_INFO("package") << "Analyzing legacy mascot archive path=\""
        << archivePath.toStdString() << "\"";
    LegacyArchiveAnalysis analysis;
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        analysis.errorMessage = QStringLiteral("Archive does not exist");
        return analysis;
    }

    try {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            analysis.errorMessage = QStringLiteral("Could not create temporary directory");
            return analysis;
        }

        auto archive = shimejifinder::analyze(archiveInfo.absoluteFilePath().toStdString());
        if (!archive) {
            analysis.errorMessage = QStringLiteral("Could not analyze archive");
            return analysis;
        }
        auto rawCandidates = rawLegacyCandidates(archiveInfo.absoluteFilePath());
        archive->extract(tempDir.path().toStdString());

        QSet<QString> seenNames;
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            seenNames.insert(qName);
            QString sourcePath = QDir(tempDir.path()).absoluteFilePath(
                qName + QStringLiteral(".mascot"));
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);
            analysis.candidates.append(inspectLegacyDirectory(sourcePath, qName));
        }
        for (auto it = rawCandidates.constBegin(); it != rawCandidates.constEnd(); ++it) {
            QString name = it.key();
            if (seenNames.contains(name)) {
                continue;
            }
            LegacyMascotCandidate candidate;
            candidate.name = name;
            candidate.metadata.name = name;
            if (!it.value().hasActions) {
                candidate.errors.append(QStringLiteral("Missing actions.xml"));
            }
            if (!it.value().hasBehaviors) {
                candidate.errors.append(QStringLiteral("Missing behaviors.xml"));
            }
            if (!it.value().hasImage) {
                candidate.errors.append(QStringLiteral("Missing img/*.png"));
            }
            if (candidate.errors.isEmpty()) {
                candidate.errors.append(QStringLiteral(
                    "Could not recognize this mascot in the archive"));
            }
            analysis.candidates.append(candidate);
        }
        if (analysis.candidates.isEmpty()) {
            analysis.errorMessage = QStringLiteral("No Shimeji mascots were found in the archive");
            return analysis;
        }

        analysis.ok = std::any_of(analysis.candidates.begin(), analysis.candidates.end(),
            [](LegacyMascotCandidate const& candidate) {
                return candidate.convertible;
            });
        if (!analysis.ok) {
            analysis.errorMessage = QStringLiteral("No convertible mascots were found");
        }
    }
    catch (std::exception const& ex) {
        analysis.errorMessage = QString::fromUtf8(ex.what());
    }
    APP_LOG_INFO("package") << "Legacy archive analysis completed path=\""
        << archivePath.toStdString() << "\" candidates="
        << analysis.candidates.size() << " ok=" << analysis.ok;
    return analysis;
}

QList<LegacyMascotConversionResult> writeLegacyArchiveSelectionAsPackages(
    QString const& archivePath, QString const& outputPath,
    QStringList const& selectedNames)
{
    APP_LOG_INFO("package") << "Converting legacy archive path=\""
        << archivePath.toStdString() << "\" output=\""
        << outputPath.toStdString() << "\" selected=" << selectedNames.size();
    QList<LegacyMascotConversionResult> results;
    QFileInfo archiveInfo(archivePath);
    QDir outputDir(outputPath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        LegacyMascotConversionResult result;
        result.errorMessage = QStringLiteral("Archive does not exist");
        results.append(result);
        return results;
    }
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        LegacyMascotConversionResult result;
        result.errorMessage = QStringLiteral("Could not create output directory");
        results.append(result);
        return results;
    }

    QSet<QString> selected;
    for (auto const& name : selectedNames) {
        selected.insert(name);
    }
    QSet<QString> processed;

    try {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            LegacyMascotConversionResult result;
            result.errorMessage = QStringLiteral("Could not create temporary directory");
            results.append(result);
            return results;
        }

        auto archive = shimejifinder::analyze(archiveInfo.absoluteFilePath().toStdString());
        if (!archive) {
            LegacyMascotConversionResult result;
            result.errorMessage = QStringLiteral("Could not analyze archive");
            results.append(result);
            return results;
        }
        archive->extract(tempDir.path().toStdString());

        QSet<QString> reservedOutputPaths;
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            if (!selected.contains(qName)) {
                continue;
            }
            processed.insert(qName);

            LegacyMascotConversionResult result;
            result.name = qName;
            QString sourcePath = QDir(tempDir.path()).absoluteFilePath(
                qName + QStringLiteral(".mascot"));
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);

            auto candidate = inspectLegacyDirectory(sourcePath, qName);
            result.name = candidate.metadata.name;
            if (!candidate.convertible) {
                result.errorMessage = candidate.errors.join(QStringLiteral("; "));
                results.append(result);
                continue;
            }

            ensureLegacyMetadata(sourcePath, qName);
            if (candidate.generatedMetadata) {
                writeFallbackMetadata(sourcePath, qName);
            }
            QString targetPath = uniquePackagePath(outputPath, candidate.metadata.name,
                reservedOutputPaths);
            if (writePackageFromDirectory(sourcePath, targetPath, result.errorMessage)) {
                result.ok = true;
                result.packagePath = targetPath;
            }
            results.append(result);
        }

        for (auto const& selectedName : selected) {
            if (!processed.contains(selectedName)) {
                LegacyMascotConversionResult result;
                result.name = selectedName;
                result.errorMessage = QStringLiteral("Selected mascot was not found");
                results.append(result);
            }
        }
    }
    catch (std::exception const& ex) {
        LegacyMascotConversionResult result;
        result.errorMessage = QString::fromUtf8(ex.what());
        results.append(result);
    }

    APP_LOG_INFO("package") << "Legacy archive conversion completed path=\""
        << archivePath.toStdString() << "\" results=" << results.size();
    return results;
}

std::set<std::string> importArchive(QString const& archivePath,
    QString const& storagePath)
{
    APP_LOG_INFO("import") << "Import archive analysis started path=\""
        << archivePath.toStdString() << "\" storage=\""
        << storagePath.toStdString() << "\"";
    std::set<std::string> imported;
    QFileInfo archiveInfo(archivePath);
    QString error;

    if (archiveInfo.suffix().compare(QStringLiteral("mascot"),
        Qt::CaseInsensitive) == 0)
    {
        QString installedName;
        if (installPackage(archiveInfo.absoluteFilePath(), storagePath,
            installedName, error))
        {
            imported.insert(installedName.toStdString());
            APP_LOG_INFO("import") << "Imported mascot package directly name=\""
                << installedName.toStdString() << "\"";
            return imported;
        }
        APP_LOG_WARN("import") << "Direct mascot package install failed path=\""
            << archiveInfo.absoluteFilePath().toStdString() << "\": "
            << error.toStdString();
    }

    try {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            APP_LOG_ERROR("import") << "Could not create temporary directory for import path=\""
                << archivePath.toStdString() << "\"";
            return imported;
        }
        auto archive = shimejifinder::analyze(archiveInfo.absoluteFilePath().toStdString());
        archive->extract(tempDir.path().toStdString());
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            QString sourcePath = QDir(tempDir.path()).absoluteFilePath(
                qName + QStringLiteral(".mascot"));
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);
            QString installedName;
            if (packageLegacyDirectory(sourcePath, storagePath, qName,
                installedName, error))
            {
                imported.insert(installedName.toStdString());
            }
            else {
                APP_LOG_WARN("import") << "Could not package legacy mascot name=\""
                    << qName.toStdString() << "\": " << error.toStdString();
            }
        }
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("import") << "Legacy import failed for path=\""
            << archivePath.toStdString() << "\": " << ex.what();
    }
    APP_LOG_INFO("import") << "Import archive analysis completed path=\""
        << archivePath.toStdString() << "\" imported=" << imported.size();
    return imported;
}

void migrateLegacyDirectories(QString const& storagePath) {
    APP_LOG_INFO("package") << "Migrating legacy mascot directories storage=\""
        << storagePath.toStdString() << "\"";
    int migrated = 0;
    int skipped = 0;
    QDirIterator iter(storagePath, QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::NoIteratorFlags);
    while (iter.hasNext()) {
        QFileInfo entry = iter.nextFileInfo();
        QString dirname = entry.fileName();
        if (!dirname.endsWith(QStringLiteral(".mascot"), Qt::CaseInsensitive) ||
            dirname.length() <= 7)
        {
            continue;
        }
        QString fallbackName = dirname.sliced(0, dirname.length() - 7);
        QString tempPackage = QDir(storagePath).absoluteFilePath(
            fallbackName + QStringLiteral(".mascot.migrating"));
        QString error;
        ensureLegacyMetadata(entry.absoluteFilePath(), fallbackName);
        if (!writePackageFromDirectory(entry.absoluteFilePath(), tempPackage, error)) {
            APP_LOG_WARN("mascot") << "Could not migrate legacy mascot directory path=\""
                << entry.absoluteFilePath().toStdString() << "\": "
                << error.toStdString();
            QFile::remove(tempPackage);
            ++skipped;
            continue;
        }
        QDir legacyDir(entry.absoluteFilePath());
        if (!legacyDir.removeRecursively()) {
            QFile::remove(tempPackage);
            ++skipped;
            continue;
        }
        QFile::rename(tempPackage, entry.absoluteFilePath());
        ++migrated;
    }
    APP_LOG_INFO("package") << "Legacy mascot migration completed migrated="
        << migrated << " skipped=" << skipped;
}

}
