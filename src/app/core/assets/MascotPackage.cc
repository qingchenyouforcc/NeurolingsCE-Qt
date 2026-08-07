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
#include "shijima-qt/SecurityLimits.hpp"

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
#include <cstring>
#include <limits>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

#include <shimejifinder/analyze.hpp>
#include <shimejifinder/extractor.hpp>
#include <shimejifinder/libunarr/archive.hpp>
#include <shimejifinder/memory_extractor.hpp>
#include <unarr/unarr.h>

namespace {

constexpr qsizetype kPortablePackageBaseNameMaxUtf8Bytes = 200;

struct RawArchiveEntryInfo {
    QString name;
    bool isDirectory = false;
    std::uint64_t uncompressedSize = 0;
};

bool readRawArchiveEntries(QString const& packagePath,
    QList<RawArchiveEntryInfo> &entries, QString &errorMessage)
{
    entries.clear();
    ar_stream *stream = ar_open_file(packagePath.toStdString().c_str());
    if (stream == nullptr) {
        errorMessage = QStringLiteral("Could not open package for inspection");
        return false;
    }
    ar_archive *archive = ar_open_zip_archive(stream, false);
    if (archive == nullptr) {
        ar_close(stream);
        errorMessage = QStringLiteral("Package is not a valid ZIP archive");
        return false;
    }
    bool ok = true;
    while (ar_parse_entry(archive)) {
        if (entries.size() >= SecurityLimits::kMascotZipEntryMaxCount) {
            errorMessage = QStringLiteral(
                "Archive contains too many entries (%1, maximum %2)")
                .arg(SecurityLimits::kMascotZipEntryMaxCount + 1)
                .arg(SecurityLimits::kMascotZipEntryMaxCount);
            ok = false;
            break;
        }
        char const *name = ar_entry_get_name(archive);
        if (name == nullptr) {
            name = ar_entry_get_raw_name(archive);
        }
        if (name == nullptr) {
            errorMessage = QStringLiteral(
                "Archive contains an entry without a name");
            ok = false;
            break;
        }
        RawArchiveEntryInfo info;
        info.name = QString::fromUtf8(name);
        info.isDirectory = info.name.endsWith(QLatin1Char('/')) ||
            info.name.endsWith(QLatin1Char('\\'));
        info.uncompressedSize =
            static_cast<std::uint64_t>(ar_entry_get_size(archive));
        entries.append(info);
    }
    ar_close_archive(archive);
    ar_close(stream);
    if (!ok) {
        return false;
    }
    return true;
}

class ExactPathExtractor : public shimejifinder::extractor {
public:
    explicit ExactPathExtractor(QString root): m_root(QDir::cleanPath(root)) {}

    void begin_write(shimejifinder::extract_target const& target) override {
        m_currentFileBytes = 0;
        QString relative;
        if (target.type() == shimejifinder::extract_target::extract_type::UNSPECIFIED) {
            relative = QString::fromStdString(target.extract_name());
        }
        else {
            relative = QString::fromStdString(target.shimeji_name()) +
                QStringLiteral(".mascot/");
            if (target.type() == shimejifinder::extract_target::extract_type::IMAGE) {
                relative += QStringLiteral("img/");
            }
            else if (target.type() == shimejifinder::extract_target::extract_type::SOUND) {
                relative += QStringLiteral("sound/");
            }
            else if (target.type() != shimejifinder::extract_target::extract_type::XML) {
                throw std::runtime_error("Unsupported package extraction target type");
            }
            relative += QString::fromStdString(target.extract_name());
        }
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
        std::uint64_t chunkSize = static_cast<std::uint64_t>(size);
        if (m_totalBytes > SecurityLimits::kMascotExtractedMaxBytes ||
            chunkSize > SecurityLimits::kMascotExtractedMaxBytes - m_totalBytes)
        {
            throw std::runtime_error("Mascot package extracted data is too large");
        }
        if (m_currentFileBytes > SecurityLimits::kMascotSingleFileMaxBytes ||
            chunkSize > SecurityLimits::kMascotSingleFileMaxBytes - m_currentFileBytes)
        {
            throw std::runtime_error("Mascot package entry is too large");
        }
        m_totalBytes += chunkSize;
        m_currentFileBytes += chunkSize;
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
    std::uint64_t m_totalBytes = 0;
    std::uint64_t m_currentFileBytes = 0;
    std::vector<std::ofstream> m_activeWrites;
};

class LimitedMemoryExtractor : public shimejifinder::extractor {
public:
    explicit LimitedMemoryExtractor(std::uint64_t maxBytes): m_maxBytes(maxBytes) {}

    void begin_write(shimejifinder::extract_target const& target) override {
        m_activeTargets.emplace_back(target.extract_name());
        m_buffer.clear();
    }

    void write_next(size_t offset, const void *buf, size_t size) override {
        std::uint64_t uOffset = static_cast<std::uint64_t>(offset);
        std::uint64_t chunkSize = static_cast<std::uint64_t>(size);
        if (uOffset > m_maxBytes || chunkSize > m_maxBytes - uOffset) {
            throw std::runtime_error("Mascot package entry is too large");
        }
        std::uint64_t end = uOffset + chunkSize;
        if (end > static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max())) {
            throw std::runtime_error("Mascot package entry is too large");
        }
        if (static_cast<qsizetype>(end) > m_buffer.size()) {
            m_buffer.resize(static_cast<qsizetype>(end));
        }
        std::memcpy(m_buffer.data() + offset, buf, size);
    }

    void end_write() override {
        for (auto const& target : m_activeTargets) {
            m_output[target] = m_buffer;
        }
        m_activeTargets.clear();
        m_buffer.clear();
    }

    bool contains(std::string const& name) const {
        return m_output.count(name) == 1;
    }

    QByteArray data(std::string const& name) const {
        if (m_output.count(name) == 1) {
            return m_output.at(name);
        }
        return {};
    }

private:
    std::uint64_t m_maxBytes;
    QByteArray m_buffer;
    std::vector<std::string> m_activeTargets;
    std::map<std::string, QByteArray> m_output;
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

bool isForbiddenPayloadPath(QString const& lowerPath)
{
    static const QStringList forbiddenExtensions = {
        QStringLiteral(".exe"), QStringLiteral(".dll"), QStringLiteral(".com"),
        QStringLiteral(".bat"), QStringLiteral(".cmd"), QStringLiteral(".ps1"),
        QStringLiteral(".sh"), QStringLiteral(".js"), QStringLiteral(".vbs"),
        QStringLiteral(".lnk"), QStringLiteral(".scr"), QStringLiteral(".pif"),
        QStringLiteral(".msi"), QStringLiteral(".msp"), QStringLiteral(".hta"),
        QStringLiteral(".jar"),
    };
    static const QStringList nestedArchiveExtensions = {
        QStringLiteral(".zip"), QStringLiteral(".mascot"), QStringLiteral(".rar"),
        QStringLiteral(".7z"), QStringLiteral(".tar"), QStringLiteral(".gz"),
        QStringLiteral(".bz2"), QStringLiteral(".xz"), QStringLiteral(".tgz"),
        QStringLiteral(".cab"), QStringLiteral(".iso"), QStringLiteral(".apk"),
        QStringLiteral(".war"), QStringLiteral(".ear"),
    };
    for (auto const& extension : forbiddenExtensions) {
        if (lowerPath.endsWith(extension)) {
            return true;
        }
    }
    for (auto const& extension : nestedArchiveExtensions) {
        if (lowerPath.endsWith(extension)) {
            return true;
        }
    }
    return false;
}

bool isAllowedPackageEntryPath(QString const& rawPath,
    QString &normalizedOut, bool *isDirectoryOut)
{
    bool isDirectory = rawPath.endsWith(QLatin1Char('/')) ||
        rawPath.endsWith(QLatin1Char('\\'));
    if (isDirectoryOut != nullptr) {
        *isDirectoryOut = isDirectory;
    }
    QString normalized = normalizedArchivePath(rawPath);
    if (normalized.isEmpty()) {
        return false;
    }
    normalizedOut = normalized;
    if (isDirectory) {
        return true;
    }
    QString lower = normalized.toLower();
    if (!isSupportedPackagePath(normalized)) {
        return false;
    }
    if (lower.startsWith(QStringLiteral("sound/"))) {
        return lower.endsWith(QStringLiteral(".wav")) ||
            lower.endsWith(QStringLiteral(".mp3")) ||
            lower.endsWith(QStringLiteral(".ogg")) ||
            lower.endsWith(QStringLiteral(".flac")) ||
            lower.endsWith(QStringLiteral(".m4a")) ||
            lower.endsWith(QStringLiteral(".aac")) ||
            lower.endsWith(QStringLiteral(".opus"));
    }
    return true;
}

QString sanitizeEntryNameForReport(QString name)
{
    for (qsizetype i = 0; i < name.size(); ++i) {
        if (name[i].unicode() < 32 || name[i].unicode() == 0x7f) {
            name[i] = QLatin1Char('_');
        }
    }
    if (name.toUtf8().size() > 200) {
        name = QString::fromUtf8(name.toUtf8().left(197)) +
            QStringLiteral("...");
    }
    return name;
}

bool pngHeaderDimensions(QByteArray const& header, std::uint64_t *widthOut,
    std::uint64_t *heightOut)
{
    static constexpr unsigned char kPngSignature[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
    };
    if (header.size() < 24 ||
        std::memcmp(header.constData(), kPngSignature, sizeof(kPngSignature)) != 0 ||
        header.mid(12, 4) != QByteArrayLiteral("IHDR"))
    {
        return false;
    }
    auto readBigEndian32 = [](char const *data) -> std::uint32_t {
        auto bytes = reinterpret_cast<unsigned char const *>(data);
        return (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            static_cast<std::uint32_t>(bytes[3]);
    };
    if (widthOut != nullptr) {
        *widthOut = readBigEndian32(header.constData() + 16);
    }
    if (heightOut != nullptr) {
        *heightOut = readBigEndian32(header.constData() + 20);
    }
    return true;
}

bool ensureFileSizeAtMost(QFileInfo const& fileInfo, std::uint64_t maxBytes,
    QString const& label, QString &errorMessage)
{
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        errorMessage = QStringLiteral("%1 does not exist").arg(label);
        return false;
    }
    if (fileInfo.size() < 0 ||
        static_cast<std::uint64_t>(fileInfo.size()) > maxBytes)
    {
        errorMessage = QStringLiteral("%1 exceeds the maximum size of %2 bytes")
            .arg(label)
            .arg(maxBytes);
        return false;
    }
    return true;
}

bool ensurePackageFileAcceptable(QString const& packagePath, QString &errorMessage)
{
    QFileInfo info(packagePath);
    return ensureFileSizeAtMost(info, SecurityLimits::kMascotPackageMaxBytes,
        QStringLiteral("Mascot package"), errorMessage);
}

bool ensureArchiveEntryCountAcceptable(shimejifinder::archive const& archive,
    QString &errorMessage)
{
    if (archive.size() > SecurityLimits::kMascotZipEntryMaxCount) {
        errorMessage = QStringLiteral("Archive contains too many entries (%1, maximum %2)")
            .arg(archive.size())
            .arg(SecurityLimits::kMascotZipEntryMaxCount);
        return false;
    }
    return true;
}

std::uint64_t maxSizeForPackagePath(QString const& path)
{
    QString lower = path.toLower();
    if (lower.startsWith(QStringLiteral("sound/"))) {
        return SecurityLimits::kMascotAudioFileMaxBytes;
    }
    return SecurityLimits::kMascotSingleFileMaxBytes;
}

QByteArray readPackageFile(QString const& packagePath, QString const& wantedPath,
    QString &errorMessage);

bool validateImageFile(QString const& filePath, QString &errorMessage)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        errorMessage = QStringLiteral("Could not inspect image dimensions for %1")
            .arg(filePath);
        return false;
    }
    QByteArray header = file.read(24);
    static constexpr unsigned char kPngSignature[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
    };
    if (header.size() < 24 ||
        std::memcmp(header.constData(), kPngSignature, sizeof(kPngSignature)) != 0 ||
        header.mid(12, 4) != QByteArrayLiteral("IHDR"))
    {
        errorMessage = QStringLiteral("Image %1 is not a valid PNG").arg(filePath);
        return false;
    }
    auto readBigEndian32 = [](char const *data) -> std::uint32_t {
        auto bytes = reinterpret_cast<unsigned char const *>(data);
        return (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            static_cast<std::uint32_t>(bytes[3]);
    };
    std::uint64_t width = readBigEndian32(header.constData() + 16);
    std::uint64_t height = readBigEndian32(header.constData() + 20);
    if (width == 0 || height == 0) {
        errorMessage = QStringLiteral("Image %1 has invalid dimensions").arg(filePath);
        return false;
    }
    if (width > SecurityLimits::kMascotImageMaxPixels / height) {
        errorMessage = QStringLiteral("Image %1 exceeds the maximum pixel count of %2")
            .arg(filePath)
            .arg(SecurityLimits::kMascotImageMaxPixels);
        return false;
    }
    std::uint64_t pixels = width * height;
    if (pixels > SecurityLimits::kMascotImageMaxPixels) {
        errorMessage = QStringLiteral("Image %1 exceeds the maximum pixel count of %2")
            .arg(filePath)
            .arg(SecurityLimits::kMascotImageMaxPixels);
        return false;
    }
    return true;
}

bool validatePngDimensions(QString const& label, QByteArray const& header,
    QString &errorMessage)
{
    static constexpr unsigned char kPngSignature[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
    };
    if (header.size() < 24 ||
        std::memcmp(header.constData(), kPngSignature, sizeof(kPngSignature)) != 0 ||
        header.mid(12, 4) != QByteArrayLiteral("IHDR"))
    {
        errorMessage = QStringLiteral("Image %1 is not a valid PNG").arg(label);
        return false;
    }
    auto readBigEndian32 = [](char const *data) -> std::uint32_t {
        auto bytes = reinterpret_cast<unsigned char const *>(data);
        return (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            static_cast<std::uint32_t>(bytes[3]);
    };
    std::uint64_t width = readBigEndian32(header.constData() + 16);
    std::uint64_t height = readBigEndian32(header.constData() + 20);
    if (width == 0 || height == 0) {
        errorMessage = QStringLiteral("Image %1 has invalid dimensions").arg(label);
        return false;
    }
    if (width > SecurityLimits::kMascotImageMaxPixels / height) {
        errorMessage = QStringLiteral("Image %1 exceeds the maximum pixel count of %2")
            .arg(label)
            .arg(SecurityLimits::kMascotImageMaxPixels);
        return false;
    }
    std::uint64_t pixels = width * height;
    if (pixels > SecurityLimits::kMascotImageMaxPixels) {
        errorMessage = QStringLiteral("Image %1 exceeds the maximum pixel count of %2")
            .arg(label)
            .arg(SecurityLimits::kMascotImageMaxPixels);
        return false;
    }
    return true;
}

bool validatePackageImageEntry(QString const& packagePath, QString const& entryPath,
    QString &errorMessage)
{
    QByteArray bytes = readPackageFile(packagePath, entryPath, errorMessage);
    if (bytes.isEmpty()) {
        return false;
    }
    QByteArray header = bytes.left(24);
    return validatePngDimensions(entryPath, header, errorMessage);
}

bool validateExtractedDirectory(QString const& rootPath, QString &errorMessage)
{
    QDir root(rootPath);
    if (!root.exists()) {
        errorMessage = QStringLiteral("Extracted archive directory is missing");
        return false;
    }

    std::uint64_t totalBytes = 0;
    std::size_t fileCount = 0;
    QDirIterator iter(rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        QFileInfo info = iter.nextFileInfo();
        QString relative = root.relativeFilePath(info.absoluteFilePath()).replace(
            QLatin1Char('\\'), QLatin1Char('/'));
        auto safePath = SafePath::safeChildPath(rootPath, relative);
        if (!safePath.has_value() ||
            QDir::cleanPath(safePath.value()) != QDir::cleanPath(info.absoluteFilePath()))
        {
            errorMessage = QStringLiteral("Archive extracted an unsafe path");
            return false;
        }
        if (info.isSymLink()) {
            errorMessage = QStringLiteral("Archive contains symbolic links");
            return false;
        }
        if (!info.isFile()) {
            continue;
        }

        ++fileCount;
        if (fileCount > SecurityLimits::kMascotZipEntryMaxCount) {
            errorMessage = QStringLiteral("Archive contains too many extracted files");
            return false;
        }

        std::uint64_t maxBytes = maxSizeForPackagePath(relative);
        if (info.size() < 0 ||
            static_cast<std::uint64_t>(info.size()) > maxBytes)
        {
            errorMessage = QStringLiteral("Extracted file %1 exceeds size limits")
                .arg(relative);
            return false;
        }

        totalBytes += static_cast<std::uint64_t>(info.size());
        if (totalBytes > SecurityLimits::kMascotExtractedMaxBytes) {
            errorMessage = QStringLiteral("Archive extracted data is too large");
            return false;
        }

        if (relative.toLower().contains(QStringLiteral("/img/")) &&
            relative.toLower().endsWith(QStringLiteral(".png")) &&
            !validateImageFile(info.absoluteFilePath(), errorMessage))
        {
            return false;
        }
    }
    return true;
}

bool extractArchiveSafely(shimejifinder::archive &archive, QString const& outputPath,
    QString &errorMessage)
{
    QDir outputDir(outputPath);
    if (outputDir.exists()) {
        outputDir.removeRecursively();
    }
    outputDir.mkpath(QStringLiteral("."));

    ExactPathExtractor extractor(outputPath);
    try {
        archive.extract(&extractor);
    }
    catch (std::exception const& ex) {
        errorMessage = QString::fromUtf8(ex.what());
        return false;
    }

    return validateExtractedDirectory(outputPath, errorMessage);
}

bool openPackage(QString const& packagePath, shimejifinder::libunarr::archive &archive,
    QString &errorMessage)
{
    if (!ensurePackageFileAcceptable(packagePath, errorMessage)) {
        return false;
    }
    try {
        archive.open(packagePath.toStdString());
        if (!ensureArchiveEntryCountAcceptable(archive, errorMessage)) {
            return false;
        }
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

    LimitedMemoryExtractor extractor(maxSizeForPackagePath(normalizedWanted));
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
    return extractor.data(key);
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
    QStringList imagePaths;
    for (size_t i = 0; i < archive.size(); ++i) {
        QString path = normalizedArchivePath(QString::fromStdString(archive.at(i)->path()));
        QString lower = path.toLower();
        hasActions = hasActions || lower == QStringLiteral("actions.xml");
        hasBehaviors = hasBehaviors || lower == QStringLiteral("behaviors.xml");
        bool isImage = lower.startsWith(QStringLiteral("img/")) &&
            lower.endsWith(QStringLiteral(".png"));
        hasImage = hasImage || isImage;
        if (isImage) {
            imagePaths.append(path);
        }
    }
    if (!hasActions || !hasBehaviors || !hasImage) {
        errorMessage = QStringLiteral("Package must contain actions.xml, behaviors.xml, and img/*.png");
        return false;
    }
    for (auto const& imagePath : imagePaths) {
        if (!validatePackageImageEntry(packagePath, imagePath, errorMessage)) {
            return false;
        }
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

QString normalizedLegacyCandidateName(QString name);

enum class LegacyPathKind {
    Unknown,
    Actions,
    Behaviors,
    Image,
};

LegacyPathKind classifyLegacyPath(QString const& lowerPath) {
    static const QStringList actionNames = {
        QStringLiteral("actions.xml"),
        QStringLiteral("action.xml"),
        QStringLiteral("one.xml"),
        QString::fromUtf8("\xe5\x8b\x95\xe4\xbd\x9c.xml"),
    };
    static const QStringList behaviorNames = {
        QStringLiteral("behaviors.xml"),
        QStringLiteral("behavior.xml"),
        QStringLiteral("two.xml"),
        QString::fromUtf8("\xe8\xa1\x8c\xe5\x8b\x95.xml"),
    };

    QString fileName = lowerPath.section(QLatin1Char('/'), -1);
    if (actionNames.contains(fileName)) {
        return LegacyPathKind::Actions;
    }
    if (behaviorNames.contains(fileName)) {
        return LegacyPathKind::Behaviors;
    }
    if (lowerPath.endsWith(QStringLiteral(".png")) &&
        (lowerPath.startsWith(QStringLiteral("img/")) ||
            lowerPath.contains(QStringLiteral("/img/"))))
    {
        return LegacyPathKind::Image;
    }
    return LegacyPathKind::Unknown;
}

QString legacyRootForPath(QString const& path, QFileInfo const& archiveInfo) {
    QString lower = path.toLower();
    if (lower.startsWith(QStringLiteral("img/"))) {
        return archiveInfo.completeBaseName();
    }
    if (!lower.contains(QLatin1Char('/'))) {
        return archiveInfo.completeBaseName();
    }

    qsizetype imgIndex = lower.indexOf(QStringLiteral("/img/"));
    if (imgIndex >= 0) {
        return normalizedLegacyCandidateName(path.left(imgIndex));
    }

    return normalizedLegacyCandidateName(path.left(path.lastIndexOf(QLatin1Char('/'))));
}

bool validateZipEntryRanges(std::vector<ZipEntry> const& entries,
    QString &errorMessage)
{
    if (entries.size() > SecurityLimits::kMascotZipEntryMaxCount ||
        entries.size() > std::numeric_limits<quint16>::max())
    {
        errorMessage = QStringLiteral("Mascot package contains too many entries");
        return false;
    }
    std::uint64_t totalSize = 0;
    for (auto const& entry : entries) {
        QByteArray name = entry.name.toUtf8();
        if (name.size() > std::numeric_limits<quint16>::max()) {
            errorMessage = QStringLiteral("Mascot package entry path is too long");
            return false;
        }
        if (entry.data.size() < 0 ||
            static_cast<std::uint64_t>(entry.data.size()) >
                std::numeric_limits<quint32>::max())
        {
            errorMessage = QStringLiteral("Mascot package entry is too large for ZIP32");
            return false;
        }
        totalSize += static_cast<std::uint64_t>(entry.data.size());
        if (totalSize > SecurityLimits::kMascotPackageMaxBytes) {
            errorMessage = QStringLiteral("Mascot package exceeds the maximum size of %1 bytes")
                .arg(SecurityLimits::kMascotPackageMaxBytes);
            return false;
        }
    }
    return true;
}

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

    if (entries.size() >= SecurityLimits::kMascotZipEntryMaxCount) {
        errorMessage = QStringLiteral("Mascot package contains too many entries");
        return false;
    }
    QFileInfo fileInfo(filePath);
    if (!ensureFileSizeAtMost(fileInfo, maxSizeForPackagePath(relative),
        QStringLiteral("Mascot package file"), errorMessage))
    {
        return false;
    }
    if (relative.toLower().startsWith(QStringLiteral("img/")) &&
        !validateImageFile(filePath, errorMessage))
    {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        errorMessage = QStringLiteral("Could not read %1").arg(filePath);
        return false;
    }

    ZipEntry entry;
    entry.name = relative;
    entry.data = file.readAll();
    if (entry.data.size() != fileInfo.size()) {
        errorMessage = QStringLiteral("Could not read all of %1").arg(filePath);
        return false;
    }
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
        LegacyPathKind kind = classifyLegacyPath(path.toLower());
        if (kind == LegacyPathKind::Unknown) {
            continue;
        }

        QString root = legacyRootForPath(path, archiveInfo);
        if (kind == LegacyPathKind::Actions) {
            candidates[root].hasActions = true;
        }
        else if (kind == LegacyPathKind::Behaviors) {
            candidates[root].hasBehaviors = true;
        }
        else if (kind == LegacyPathKind::Image) {
            candidates[root].hasImage = true;
        }
    }
    return candidates;
}

bool writeLocalEntry(QFile &file, ZipEntry &entry, QString &errorMessage) {
    QByteArray name = entry.name.toUtf8();
    if (file.pos() < 0 ||
        static_cast<std::uint64_t>(file.pos()) > std::numeric_limits<quint32>::max())
    {
        errorMessage = QStringLiteral("Mascot package is too large for ZIP32 offsets");
        return false;
    }
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
    return file.error() == QFileDevice::NoError;
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

void tryExtractInfoJson(QString const& archivePath, QString const& mascotName,
    QString const& targetPath)
{
    QString targetFile = QDir(targetPath).absoluteFilePath(
        QStringLiteral("info.json"));
    if (QFile::exists(targetFile)) {
        return;
    }

    shimejifinder::libunarr::archive archive;
    QString error;
    if (!openPackage(archivePath, archive, error)) {
        return;
    }

    QString mascot = mascotName.toLower();
    QString mascotDirectory = mascot + QStringLiteral(".mascot");
    QString rootMatch;
    QString mascotMatch;
    for (size_t i = 0; i < archive.size(); ++i) {
        QString path = normalizedArchivePath(
            QString::fromStdString(archive.at(i)->path()));
        QStringList parts = path.toLower().split(QLatin1Char('/'),
            Qt::SkipEmptyParts);
        if (parts.isEmpty() || parts.constLast() != QStringLiteral("info.json")) {
            continue;
        }
        if (parts.size() == 1) {
            rootMatch = path;
        }
        else if (parts[parts.size() - 2] == mascot ||
            parts[parts.size() - 2] == mascotDirectory)
        {
            mascotMatch = path;
            break;
        }
    }

    QString sourcePath = mascotMatch.isEmpty() ? rootMatch : mascotMatch;
    if (sourcePath.isEmpty()) {
        return;
    }
    QByteArray bytes = readPackageFile(archivePath, sourcePath, error);
    if (bytes.isEmpty()) {
        return;
    }
    QSaveFile out(targetFile);
    if (out.open(QFile::WriteOnly)) {
        out.write(bytes);
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

bool legacyPreviewPathMatchesMascot(QString const& path,
    QString const& mascotName)
{
    QStringList parts = path.toLower().split(QLatin1Char('/'),
        Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    qsizetype imgIndex = parts.indexOf(QStringLiteral("img"));
    if (imgIndex < 0 || imgIndex >= parts.size() - 1) {
        return false;
    }

    QString mascot = mascotName.toLower();
    QString mascotDirectory = mascot + QStringLiteral(".mascot");
    if (imgIndex == 0 && parts.size() == 2) {
        return true;
    }
    if (imgIndex > 0 &&
        (parts[imgIndex - 1] == mascot ||
            parts[imgIndex - 1] == mascotDirectory))
    {
        return true;
    }
    return imgIndex + 2 < parts.size() &&
        (parts[imgIndex + 1] == mascot ||
            parts[imgIndex + 1] == mascotDirectory);
}

void tryExtractPreviewImages(QString const& archivePath,
    QString const& mascotName, QString const& targetPath)
{
    shimejifinder::libunarr::archive archive;
    QString error;
    if (!openPackage(archivePath, archive, error)) {
        return;
    }

    QDir imgDir(QDir(targetPath).absoluteFilePath(QStringLiteral("img")));
    static QStringList const previewFileNames {
        QStringLiteral("a.png"),
        QStringLiteral("cover.png"),
    };
    for (auto const& fileName : previewFileNames) {
        if (QFile::exists(imgDir.absoluteFilePath(fileName))) {
            continue;
        }

        QString sourcePath;
        for (size_t i = 0; i < archive.size(); ++i) {
            QString path = normalizedArchivePath(
                QString::fromStdString(archive.at(i)->path()));
            if (path.section(QLatin1Char('/'), -1).compare(fileName,
                    Qt::CaseInsensitive) == 0 &&
                legacyPreviewPathMatchesMascot(path, mascotName))
            {
                sourcePath = path;
                break;
            }
        }
        if (sourcePath.isEmpty()) {
            continue;
        }

        QByteArray bytes = readPackageFile(archivePath, sourcePath, error);
        if (bytes.isEmpty() ||
            (!imgDir.exists() && !QDir().mkpath(imgDir.absolutePath())))
        {
            continue;
        }
        QSaveFile out(imgDir.absoluteFilePath(fileName));
        if (out.open(QFile::WriteOnly)) {
            out.write(bytes);
            out.commit();
        }
    }
}

LegacyMascotCandidate inspectLegacyDirectory(QString const& sourcePath,
    QString const& fallbackName)
{
    LegacyMascotCandidate candidate;
    candidate.sourceName = fallbackName;
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
    bool loadedInfoJson = info.exists() && info.open(QFile::ReadOnly);
    if (loadedInfoJson) {
        candidate.infoJson = info.readAll();
        try {
            candidate.metadata = MascotPackage::metadataFromJson(candidate.infoJson);
            candidate.name = candidate.metadata.name;
            candidate.infoJsonValid =
                MascotPackage::isValidPackageName(candidate.metadata.name);
            if (!candidate.infoJsonValid) {
                candidate.infoJsonError = QStringLiteral(
                    "Edited info.json has an invalid package name");
            }
        }
        catch (std::exception const& ex) {
            candidate.generatedMetadata = true;
            candidate.infoJsonError = QString::fromUtf8(ex.what());
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
    if (candidate.generatedMetadata && !loadedInfoJson) {
        candidate.infoJson = MascotPackage::metadataToJson(candidate.metadata);
        candidate.infoJsonValid =
            MascotPackage::isValidPackageName(candidate.metadata.name);
        if (!candidate.infoJsonValid) {
            candidate.infoJsonError = QStringLiteral(
                "Edited info.json has an invalid package name");
        }
    }
    candidate.convertible = candidate.errors.isEmpty();
    return candidate;
}

QString extractedLegacyMascotPath(QString const& extractionRoot, QString const& name)
{
    QStringList candidates {
        name + QStringLiteral(".mascot"),
        name,
    };
    for (auto const& relative : candidates) {
        auto path = SafePath::safeChildPath(extractionRoot, relative);
        if (path.has_value() && QFileInfo(path.value()).isDir()) {
            return path.value();
        }
    }
    auto fallback = SafePath::safeChildPath(extractionRoot, candidates.first());
    return fallback.value_or(QString());
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

bool isValidPackageName(QString const& name)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    QString const baseName = sanitizedPackageBaseName(name);
    if (baseName.toUtf8().size() > kPortablePackageBaseNameMaxUtf8Bytes) {
        return false;
    }
    QString const deviceName = baseName.section(QLatin1Char('.'), 0, 0).toUpper();
    if (deviceName == QStringLiteral("CON") ||
        deviceName == QStringLiteral("PRN") ||
        deviceName == QStringLiteral("AUX") ||
        deviceName == QStringLiteral("NUL"))
    {
        return false;
    }
    if (deviceName.size() == 4 &&
        (deviceName.startsWith(QStringLiteral("COM")) ||
            deviceName.startsWith(QStringLiteral("LPT"))) &&
        deviceName.at(3) >= QLatin1Char('1') &&
        deviceName.at(3) <= QLatin1Char('9'))
    {
        return false;
    }
    return true;
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

bool validatePackage(QString const& packagePath, MascotPackageReport &report)
{
    report = MascotPackageReport {};
    QFileInfo packageInfo(packagePath);
    if (!packageInfo.exists() || !packageInfo.isFile()) {
        report.errors.append(QStringLiteral("Mascot package does not exist"));
        return false;
    }
    QString errorMessage;
    if (!ensurePackageFileAcceptable(packagePath, errorMessage)) {
        report.errors.append(errorMessage);
        return false;
    }

    QList<RawArchiveEntryInfo> rawEntries;
    if (!readRawArchiveEntries(packagePath, rawEntries, errorMessage)) {
        report.errors.append(errorMessage);
        return false;
    }
    report.entryCount = static_cast<qsizetype>(rawEntries.size());

    bool hasInfo = false;
    bool hasActions = false;
    bool hasBehaviors = false;
    bool hasImage = false;
    QStringList imagePaths;
    std::uint64_t totalImagePixels = 0;
    std::uint64_t totalUncompressedBytes = 0;
    qsizetype fileCount = 0;

    for (auto const& rawEntry : rawEntries) {
        QString rawPath = rawEntry.name;
        QString normalized = normalizedArchivePath(rawPath);
        if (normalized.isEmpty()) {
            report.errors.append(QStringLiteral(
                "Unsupported or unsafe package entry: %1")
                .arg(sanitizeEntryNameForReport(rawPath)));
            continue;
        }
        bool isDirectory = rawPath.endsWith(QLatin1Char('/')) ||
            rawPath.endsWith(QLatin1Char('\\'));
        if (isDirectory) {
            continue;
        }
        ++fileCount;
        if (rawEntry.uncompressedSize >
                SecurityLimits::kMascotExtractedMaxBytes - totalUncompressedBytes)
        {
            report.errors.append(QStringLiteral(
                "Package extracted data is too large"));
            break;
        }
        totalUncompressedBytes += rawEntry.uncompressedSize;
        std::uint64_t maxEntryBytes = maxSizeForPackagePath(normalized);
        if (rawEntry.uncompressedSize > maxEntryBytes) {
            report.errors.append(QStringLiteral(
                "Package entry %1 exceeds size limits")
                .arg(sanitizeEntryNameForReport(normalized)));
        }
        QString lower = normalized.toLower();
        if (isForbiddenPayloadPath(lower)) {
            report.errors.append(QStringLiteral(
                "Package contains a forbidden payload entry: %1")
                .arg(sanitizeEntryNameForReport(normalized)));
            continue;
        }
        if (!isSupportedPackagePath(normalized) ||
            (lower.startsWith(QStringLiteral("sound/")) &&
                !isAllowedPackageEntryPath(rawPath, normalized, nullptr)))
        {
            report.errors.append(QStringLiteral(
                "Unsupported or unsafe package entry: %1")
                .arg(sanitizeEntryNameForReport(normalized)));
            continue;
        }
        hasInfo = hasInfo || lower == QStringLiteral("info.json");
        hasActions = hasActions || lower == QStringLiteral("actions.xml");
        hasBehaviors = hasBehaviors || lower == QStringLiteral("behaviors.xml");
        bool isImage = lower.startsWith(QStringLiteral("img/")) &&
            lower.endsWith(QStringLiteral(".png"));
        hasImage = hasImage || isImage;
        if (isImage) {
            imagePaths.append(normalized);
        }
    }
    report.fileCount = fileCount;
    report.extractedBytes = totalUncompressedBytes;

    if (!hasInfo) {
        report.errors.append(QStringLiteral("Package must contain info.json"));
    }
    if (!hasActions) {
        report.errors.append(QStringLiteral("Package must contain actions.xml"));
    }
    if (!hasBehaviors) {
        report.errors.append(QStringLiteral("Package must contain behaviors.xml"));
    }
    if (!hasImage) {
        report.errors.append(QStringLiteral("Package must contain img/*.png"));
    }

    if (hasInfo) {
        QByteArray infoJson = readPackageFile(packagePath,
            QStringLiteral("info.json"), errorMessage);
        if (infoJson.isEmpty()) {
            report.errors.append(errorMessage);
        }
        else {
            try {
                report.metadata = metadataFromJson(infoJson);
            }
            catch (std::exception const& ex) {
                report.errors.append(QString::fromUtf8(ex.what()));
            }
            catch (...) {
                report.errors.append(QStringLiteral("Invalid info.json"));
            }
        }
    }

    for (auto const& imagePath : imagePaths) {
        QByteArray header = readPackageFile(packagePath, imagePath, errorMessage);
        if (header.isEmpty()) {
            report.errors.append(errorMessage);
            continue;
        }
        std::uint64_t width = 0;
        std::uint64_t height = 0;
        if (!pngHeaderDimensions(header.left(24), &width, &height) ||
            width == 0 || height == 0)
        {
            report.errors.append(QStringLiteral(
                "Image %1 is not a valid PNG").arg(imagePath));
            continue;
        }
        if (width > SecurityLimits::kMascotImageMaxPixels / height) {
            report.errors.append(QStringLiteral(
                "Image %1 exceeds the maximum pixel count of %2")
                .arg(imagePath)
                .arg(SecurityLimits::kMascotImageMaxPixels));
            continue;
        }
        std::uint64_t pixels = width * height;
        if (pixels > SecurityLimits::kMascotImageMaxPixels) {
            report.errors.append(QStringLiteral(
                "Image %1 exceeds the maximum pixel count of %2")
                .arg(imagePath)
                .arg(SecurityLimits::kMascotImageMaxPixels));
            continue;
        }
        totalImagePixels += pixels;
        if (totalImagePixels > SecurityLimits::kMascotImageTotalMaxPixels) {
            report.errors.append(QStringLiteral(
                "Package image data exceeds the total pixel budget of %1")
                .arg(SecurityLimits::kMascotImageTotalMaxPixels));
            break;
        }
    }

    if (report.errors.isEmpty()) {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            report.errors.append(QStringLiteral(
                "Could not create temporary extraction directory"));
        }
        else {
            shimejifinder::libunarr::archive extractionArchive;
            if (!openPackage(packagePath, extractionArchive, errorMessage)) {
                report.errors.append(errorMessage);
            }
            {
                bool added = false;
                for (size_t i = 0; i < extractionArchive.size(); ++i) {
                    auto entry = extractionArchive.at(i);
                    QString path = normalizedArchivePath(
                        QString::fromStdString(entry->path()));
                    if (path.isEmpty() || !isSupportedPackagePath(path)) {
                        continue;
                    }
                    entry->add_target(shimejifinder::extract_target(
                        path.toStdString()));
                    added = true;
                }
                if (!added) {
                    report.errors.append(QStringLiteral(
                        "Package does not contain any supported files"));
                }
                else if (!extractArchiveSafely(extractionArchive,
                        tempDir.path(), errorMessage))
                {
                    report.errors.append(errorMessage);
                }
            }
        }
    }

    report.ok = report.errors.isEmpty();
    APP_LOG_INFO("package") << "Validated mascot package path=\""
        << packageInfo.absoluteFilePath().toStdString()
        << "\" ok=" << (report.ok ? "1" : "0")
        << " entries=" << report.entryCount
        << " files=" << report.fileCount
        << " extracted_bytes=" << report.extractedBytes
        << " errors=" << report.errors.size();
    return report.ok;
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
    if (!validateZipEntryRanges(entries, errorMessage)) {
        return false;
    }

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
        if (!writeLocalEntry(file, entry, errorMessage)) {
            if (errorMessage.isEmpty()) {
                errorMessage = QStringLiteral("Could not write package entry");
            }
            QFile::remove(packagePath);
            return false;
        }
    }
    if (file.pos() < 0 ||
        static_cast<std::uint64_t>(file.pos()) > std::numeric_limits<quint32>::max())
    {
        errorMessage = QStringLiteral("Mascot package is too large for ZIP32 central directory");
        QFile::remove(packagePath);
        return false;
    }
    quint32 centralOffset = static_cast<quint32>(file.pos());
    for (auto const& entry : entries) {
        writeCentralEntry(file, entry);
    }
    if (file.pos() < 0 ||
        static_cast<std::uint64_t>(file.pos()) > std::numeric_limits<quint32>::max())
    {
        errorMessage = QStringLiteral("Mascot package central directory is too large");
        QFile::remove(packagePath);
        return false;
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
    if (!ensurePackageFileAcceptable(packagePath, errorMessage)) {
        return false;
    }
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
    if (archiveInfo.size() < 0 ||
        static_cast<std::uint64_t>(archiveInfo.size()) >
            SecurityLimits::kMascotPackageMaxBytes)
    {
        analysis.errorMessage = QStringLiteral("Archive exceeds the maximum size of %1 bytes")
            .arg(SecurityLimits::kMascotPackageMaxBytes);
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
        if (!extractArchiveSafely(*archive, tempDir.path(), analysis.errorMessage)) {
            return analysis;
        }

        QSet<QString> seenNames;
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            seenNames.insert(qName);
            QString sourcePath = extractedLegacyMascotPath(tempDir.path(), qName);
            tryExtractInfoJson(archiveInfo.absoluteFilePath(), qName, sourcePath);
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);
            analysis.candidates.append(inspectLegacyDirectory(sourcePath, qName));
        }
        for (auto it = rawCandidates.constBegin(); it != rawCandidates.constEnd(); ++it) {
            QString name = it.key();
            if (seenNames.contains(name)) {
                continue;
            }
            LegacyMascotCandidate candidate;
            candidate.sourceName = name;
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
    return writeLegacyArchiveSelectionAsPackages(
        archivePath, outputPath, selectedNames, {});
}

QList<LegacyMascotConversionResult> writeLegacyArchiveSelectionAsPackages(
    QString const& archivePath, QString const& outputPath,
    QStringList const& selectedNames,
    QHash<QString, QByteArray> const& infoJsonOverrides)
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
    if (archiveInfo.size() < 0 ||
        static_cast<std::uint64_t>(archiveInfo.size()) >
            SecurityLimits::kMascotPackageMaxBytes)
    {
        LegacyMascotConversionResult result;
        result.errorMessage = QStringLiteral("Archive exceeds the maximum size of %1 bytes")
            .arg(SecurityLimits::kMascotPackageMaxBytes);
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
        QString errorMessage;
        if (!extractArchiveSafely(*archive, tempDir.path(), errorMessage)) {
            LegacyMascotConversionResult result;
            result.errorMessage = errorMessage;
            results.append(result);
            return results;
        }

        QSet<QString> reservedOutputPaths;
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            if (!selected.contains(qName)) {
                continue;
            }
            processed.insert(qName);

            LegacyMascotConversionResult result;
            result.name = qName;
            QString sourcePath = extractedLegacyMascotPath(tempDir.path(), qName);
            tryExtractInfoJson(archiveInfo.absoluteFilePath(), qName, sourcePath);
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);
            tryExtractPreviewImages(archiveInfo.absoluteFilePath(), qName,
                sourcePath);

            auto candidate = inspectLegacyDirectory(sourcePath, qName);
            if (!candidate.convertible) {
                result.errorMessage = candidate.errors.join(QStringLiteral("; "));
                results.append(result);
                continue;
            }

            auto overrideIt = infoJsonOverrides.constFind(qName);
            if (overrideIt != infoJsonOverrides.constEnd()) {
                if (static_cast<std::uint64_t>(overrideIt->size()) >
                    SecurityLimits::kMascotSingleFileMaxBytes)
                {
                    result.errorMessage = QStringLiteral("Edited info.json is too large");
                    results.append(result);
                    continue;
                }
                try {
                    candidate.metadata = metadataFromJson(*overrideIt);
                }
                catch (...) {
                    result.errorMessage = QStringLiteral("Edited info.json is invalid");
                    results.append(result);
                    continue;
                }
                if (!isValidPackageName(candidate.metadata.name)) {
                    result.errorMessage = QStringLiteral(
                        "Edited info.json has an invalid package name");
                    results.append(result);
                    continue;
                }
                QSaveFile infoFile(QDir(sourcePath).absoluteFilePath(
                    QStringLiteral("info.json")));
                if (!infoFile.open(QFile::WriteOnly) ||
                    infoFile.write(*overrideIt) != overrideIt->size() ||
                    !infoFile.commit())
                {
                    result.errorMessage = QStringLiteral("Could not write edited info.json");
                    results.append(result);
                    continue;
                }
            }
            else {
                ensureLegacyMetadata(sourcePath, qName);
                if (candidate.generatedMetadata) {
                    writeFallbackMetadata(sourcePath, qName);
                }
            }
            result.name = candidate.metadata.name;
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
    if (!archiveInfo.exists()) {
        APP_LOG_WARN("import") << "Archive rejected because path does not exist path=\""
            << archivePath.toStdString() << "\"";
        return imported;
    }

    if (archiveInfo.isDir()) {
        QString installedName;
        if (packageLegacyDirectory(archiveInfo.absoluteFilePath(), storagePath,
            archiveInfo.fileName(), installedName, error))
        {
            imported.insert(installedName.toStdString());
            APP_LOG_INFO("import") << "Imported mascot template directory name=\""
                << installedName.toStdString() << "\"";
        }
        else {
            APP_LOG_WARN("import") << "Template directory import failed path=\""
                << archiveInfo.absoluteFilePath().toStdString() << "\": "
                << error.toStdString();
        }
        return imported;
    }

    if (!archiveInfo.isFile() ||
        archiveInfo.size() < 0 ||
        static_cast<std::uint64_t>(archiveInfo.size()) >
            SecurityLimits::kMascotPackageMaxBytes)
    {
        APP_LOG_WARN("import") << "Archive rejected by size limit path=\""
            << archivePath.toStdString() << "\"";
        return imported;
    }

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
        if (!archive) {
            APP_LOG_WARN("import") << "Could not analyze archive path=\""
                << archiveInfo.absoluteFilePath().toStdString() << "\"";
            return imported;
        }
        if (!extractArchiveSafely(*archive, tempDir.path(), error)) {
            APP_LOG_WARN("import") << "Legacy archive extraction failed path=\""
                << archiveInfo.absoluteFilePath().toStdString() << "\": "
                << error.toStdString();
            return imported;
        }
        for (auto const& name : archive->shimejis()) {
            QString qName = QString::fromStdString(name);
            QString sourcePath = extractedLegacyMascotPath(tempDir.path(), qName);
            tryExtractInfoJson(archiveInfo.absoluteFilePath(), qName, sourcePath);
            tryExtractBubbleContext(archiveInfo.absoluteFilePath(), qName, sourcePath);
            tryExtractPreviewImages(archiveInfo.absoluteFilePath(), qName,
                sourcePath);
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
