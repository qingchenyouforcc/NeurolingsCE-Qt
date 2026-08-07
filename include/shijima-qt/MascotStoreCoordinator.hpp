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

#include "shijima-qt/MascotStoreCache.hpp"
#include "shijima-qt/MascotStoreIndex.hpp"
#include "shijima-qt/MascotStoreNetwork.hpp"

#include <QObject>
#include <QString>

class MascotStoreNetwork;
class QTimer;

class MascotStoreCoordinator : public QObject
{
    Q_OBJECT
public:
    struct IndexState {
        bool loaded = false;
        bool fromCache = false;
        bool stale = false;
        QString errorCode;
        QString error;
        MascotStoreIndex index;
    };

    explicit MascotStoreCoordinator(MascotStoreCache *cache,
        MascotStoreNetwork *network, QString mascotStoragePath,
        QString downloadCachePath, QObject *parent = nullptr);

    void refreshIndex();
    void loadCachedIndex();
    // Downloads + verifies + installs the entry. Blocking install work runs
    // off the GUI thread; the result is delivered via entryFinished().
    void downloadAndInstall(MascotStoreEntry const& entry);
    void cancelDownload(QString const& mascotId);
    bool isDownloading(QString const& mascotId) const;

signals:
    void indexStateChanged(MascotStoreCoordinator::IndexState state);
    void entryProgress(QString mascotId, qint64 received, qint64 total);
    void entryFinished(QString mascotId, bool ok, QString installedName,
        QString errorCode, QString error);

private slots:
    void onIndexFetched(MascotStoreIndexResponse response);
    void onDownloadProgress(QString destinationPath, qint64 received,
        qint64 total);
    void onDownloadFinished(QString destinationPath, bool ok,
        QString errorCode, QString error);
    void onInstallFinished(QString mascotId, bool ok, QString installedName,
        QString errorCode, QString error);

private:
    void emitCachedIndex();
    QString cacheKeyFor(QString const& destinationPath) const;

    MascotStoreCache *m_cache = nullptr;
    MascotStoreNetwork *m_network = nullptr;
    QString m_mascotStoragePath;
    QString m_downloadCachePath;
    QString m_activeDownloadId;
    QString m_activeDownloadPath;
    int m_retryCount = 0;
};
