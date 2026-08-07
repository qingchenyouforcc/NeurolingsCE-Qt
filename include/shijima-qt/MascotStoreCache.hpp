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

#include <QByteArray>
#include <QString>

class MascotStoreCache {
public:
    struct CachedIndex {
        QByteArray body;
        QString etag;
        QString lastModified;
    };

    explicit MascotStoreCache(QString cacheRoot);

    bool loadIndex(CachedIndex *out) const;
    bool loadPreviousIndex(CachedIndex *out) const;
    // Atomically saves the index; the previously good index is kept as
    // "previous" so a corrupt refresh never destroys the last good cache.
    bool saveIndex(CachedIndex const& index, QString *error = nullptr);

    QString indexFilePath() const;
    QString previousIndexFilePath() const;
    QString metadataFilePath() const;

private:
    bool readIndexFile(QString const& path, CachedIndex *out) const;
    QString m_cacheRoot;
};
