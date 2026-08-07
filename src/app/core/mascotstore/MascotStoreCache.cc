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

#include "shijima-qt/MascotStoreCache.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QString cacheFileName() {
    return QStringLiteral("index-v1.json");
}

QString previousCacheFileName() {
    return QStringLiteral("index-v1.previous.json");
}

QString metadataFileName() {
    return QStringLiteral("index-v1.meta");
}

}  // namespace

MascotStoreCache::MascotStoreCache(QString cacheRoot):
    m_cacheRoot(cacheRoot)
{
}

bool MascotStoreCache::readIndexFile(QString const& path,
    CachedIndex *out) const
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }
    CachedIndex index;
    index.body = file.readAll();
    file.close();
    if (index.body.isEmpty()) {
        return false;
    }
    QFile metaFile(QDir(m_cacheRoot).filePath(metadataFileName()));
    if (metaFile.open(QFile::ReadOnly)) {
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(
            metaFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError &&
            document.isObject())
        {
            QJsonObject object = document.object();
            index.etag = object.value(QStringLiteral("etag")).toString();
            index.lastModified = object.value(
                QStringLiteral("last_modified")).toString();
        }
    }
    if (out != nullptr) {
        *out = index;
    }
    return true;
}

bool MascotStoreCache::loadIndex(CachedIndex *out) const {
    return readIndexFile(QDir(m_cacheRoot).filePath(cacheFileName()), out);
}

bool MascotStoreCache::loadPreviousIndex(CachedIndex *out) const {
    return readIndexFile(QDir(m_cacheRoot).filePath(previousCacheFileName()), out);
}

bool MascotStoreCache::saveIndex(CachedIndex const& index, QString *error) {
    QDir dir(m_cacheRoot);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create the store cache directory");
        }
        return false;
    }
    QString currentPath = dir.filePath(cacheFileName());
    QString previousPath = dir.filePath(previousCacheFileName());
    if (QFileInfo::exists(currentPath)) {
        QFile::remove(previousPath);
        if (!QFile::copy(currentPath, previousPath)) {
            if (error != nullptr) {
                *error = QStringLiteral("Could not preserve the previous index cache");
            }
            return false;
        }
    }

    QSaveFile indexFile(currentPath);
    if (!indexFile.open(QFile::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write the index cache");
        }
        return false;
    }
    indexFile.write(index.body);
    if (!indexFile.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not atomically save the index cache");
        }
        return false;
    }

    QJsonObject meta;
    meta[QStringLiteral("etag")] = index.etag;
    meta[QStringLiteral("last_modified")] = index.lastModified;
    QSaveFile metaFile(dir.filePath(metadataFileName()));
    if (!metaFile.open(QFile::WriteOnly)) {
        return true;  // index is saved; metadata is best-effort
    }
    metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    metaFile.commit();
    return true;
}

QString MascotStoreCache::indexFilePath() const {
    return QDir(m_cacheRoot).filePath(cacheFileName());
}

QString MascotStoreCache::previousIndexFilePath() const {
    return QDir(m_cacheRoot).filePath(previousCacheFileName());
}

QString MascotStoreCache::metadataFilePath() const {
    return QDir(m_cacheRoot).filePath(metadataFileName());
}
