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

#include "shijima-qt/MascotStoreCoordinator.hpp"

#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/MascotStoreConfig.hpp"
#include "shijima-qt/MascotStoreNetwork.hpp"
#include "shijima-qt/SafePath.hpp"

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrent>

#include <tuple>

namespace {

int const kMaxIndexRetries = 2;
int const kMaxDownloadRetries = 2;

QString sanitizedCacheBaseName(QString const& mascotId) {
    QString result = mascotId.trimmed();
    for (qsizetype i = 0; i < result.size(); ++i) {
        QChar ch = result[i];
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-') &&
            ch != QLatin1Char('_') && ch != QLatin1Char('.'))
        {
            result[i] = QLatin1Char('_');
        }
    }
    if (result.isEmpty()) {
        result = QStringLiteral("mascot");
    }
    return result;
}

bool isTrustedDownloadUrl(QUrl const& url) {
    if (url.scheme() == QStringLiteral("https")) {
        return true;
    }
    if (url.scheme() == QStringLiteral("http")) {
        QString host = url.host().toLower();
        return host == QStringLiteral("localhost") ||
            host == QStringLiteral("127.0.0.1") ||
            host == QStringLiteral("::1");
    }
    return false;
}

}  // namespace

MascotStoreCoordinator::MascotStoreCoordinator(MascotStoreCache *cache,
    MascotStoreNetwork *network, QString mascotStoragePath,
    QString downloadCachePath, QObject *parent):
    QObject(parent),
    m_cache(cache),
    m_network(network),
    m_mascotStoragePath(mascotStoragePath),
    m_downloadCachePath(downloadCachePath)
{
    connect(m_network, &MascotStoreNetwork::indexFetched,
        this, &MascotStoreCoordinator::onIndexFetched);
    connect(m_network, &MascotStoreNetwork::downloadProgress,
        this, &MascotStoreCoordinator::onDownloadProgress);
    connect(m_network, &MascotStoreNetwork::downloadFinished,
        this, &MascotStoreCoordinator::onDownloadFinished);
}

void MascotStoreCoordinator::refreshIndex() {
    if (!MascotStoreConfig::isConfigured()) {
        IndexState state;
        state.errorCode = QStringLiteral("mascotstore.not_configured");
        state.error = QStringLiteral(
            "The mascot store is not configured by the maintainer");
        emit indexStateChanged(state);
        return;
    }
    MascotStoreCache::CachedIndex cached;
    QString etag;
    QString lastModified;
    if (m_cache->loadIndex(&cached)) {
        etag = cached.etag;
        lastModified = cached.lastModified;
    }
    m_retryCount = 0;
    m_network->fetchIndex(QUrl { MascotStoreConfig::indexUrl() }, etag,
        lastModified);
}

void MascotStoreCoordinator::loadCachedIndex() {
    emitCachedIndex();
}

void MascotStoreCoordinator::emitCachedIndex() {
    MascotStoreCache::CachedIndex cached;
    MascotStoreIndex index;
    QString error;
    if (m_cache->loadIndex(&cached) && index.parse(cached.body, &error)) {
        IndexState state;
        state.loaded = true;
        state.fromCache = true;
        state.index = index;
        emit indexStateChanged(state);
        return;
    }
    if (m_cache->loadPreviousIndex(&cached) && index.parse(cached.body, &error)) {
        IndexState state;
        state.loaded = true;
        state.fromCache = true;
        state.stale = true;
        state.index = index;
        emit indexStateChanged(state);
        return;
    }
    IndexState state;
    state.errorCode = QStringLiteral("mascotstore.cache.empty");
    state.error = QStringLiteral("No cached mascot index is available offline");
    emit indexStateChanged(state);
}

void MascotStoreCoordinator::onIndexFetched(MascotStoreIndexResponse response) {
    if (!response.ok) {
        bool transient = response.errorCode ==
            QStringLiteral("mascotstore.timeout") ||
            response.errorCode == QStringLiteral("mascotstore.network");
        if (transient && m_retryCount < kMaxIndexRetries) {
            ++m_retryCount;
            APP_LOG_WARN("mascotstore") << "Retrying index fetch attempt="
                << m_retryCount;
            QTimer::singleShot(1000 * m_retryCount, this, [this]() {
                MascotStoreCache::CachedIndex cached;
                QString etag;
                QString lastModified;
                if (m_cache->loadIndex(&cached)) {
                    etag = cached.etag;
                    lastModified = cached.lastModified;
                }
                m_network->fetchIndex(
                    QUrl { MascotStoreConfig::indexUrl() }, etag, lastModified);
            });
            return;
        }
        // Offline fallback: show the last successful cache.
        IndexState state;
        MascotStoreCache::CachedIndex cached;
        MascotStoreIndex index;
        QString error;
        if (m_cache->loadIndex(&cached) && index.parse(cached.body, &error)) {
            state.loaded = true;
            state.fromCache = true;
            state.stale = true;
            state.index = index;
        }
        state.errorCode = response.errorCode;
        state.error = response.error;
        emit indexStateChanged(state);
        return;
    }
    if (response.notModified) {
        IndexState state;
        MascotStoreCache::CachedIndex cached;
        MascotStoreIndex index;
        QString error;
        if (m_cache->loadIndex(&cached) && index.parse(cached.body, &error)) {
            state.loaded = true;
            state.fromCache = true;
            state.index = index;
        }
        else {
            state.errorCode = QStringLiteral("mascotstore.cache.corrupt");
            state.error = QStringLiteral(
                "The cached index is corrupt and the server reported no changes");
        }
        emit indexStateChanged(state);
        return;
    }

    MascotStoreIndex index;
    QString error;
    if (!index.parse(response.body, &error)) {
        IndexState state;
        // Preserve the previous good cache; do not overwrite it.
        state.errorCode = QStringLiteral("mascotstore.index.invalid");
        state.error = error;
        emit indexStateChanged(state);
        return;
    }
    MascotStoreCache::CachedIndex cached;
    cached.body = response.body;
    cached.etag = response.etag;
    cached.lastModified = response.lastModified;
    m_cache->saveIndex(cached);

    IndexState state;
    state.loaded = true;
    state.index = index;
    emit indexStateChanged(state);
}

void MascotStoreCoordinator::downloadAndInstall(MascotStoreEntry const& entry) {
    // MascotStoreNetwork owns one package transfer at a time. Refuse a second
    // request instead of letting it cancel the first transfer and misattribute
    // the resulting finished signal to the newly selected entry.
    if (hasActiveOperation()) {
        return;
    }
    if (!entry.download.url.isValid() ||
        !isTrustedDownloadUrl(entry.download.url))
    {
        emit entryFinished(entry.id, false, {},
            QStringLiteral("mascotstore.download.invalid_url"),
            QStringLiteral("The download URL is invalid"));
        return;
    }
    QString baseName = sanitizedCacheBaseName(entry.id) +
        QStringLiteral("-") + entry.version + QStringLiteral(".mascot");
    QDir cacheDir(m_downloadCachePath);
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
        emit entryFinished(entry.id, false, {},
            QStringLiteral("mascotstore.download.cache"),
            QStringLiteral("Could not create the store download cache"));
        return;
    }
    QString targetPath = cacheDir.filePath(baseName);
    m_activeDownloadId = entry.id;
    m_activeDownloadPath = targetPath;
    m_retryCount = 0;
    m_network->download(entry.download.url, targetPath, entry.download.sha256);
}

void MascotStoreCoordinator::cancelDownload(QString const& mascotId) {
    if (mascotId == m_activeDownloadId) {
        // MascotStoreNetwork emits the canceled completion synchronously after
        // it has invalidated the reply. Keep the active id until that callback
        // runs so the coordinator reports the correct entry.
        m_network->cancelDownload();
        if (m_activeDownloadId == mascotId) {
            // A transient failure may be waiting in the retry backoff with no
            // network reply left to cancel. Clear that operation explicitly
            // and invalidate its queued retry callback.
            m_activeDownloadId.clear();
            m_activeDownloadPath.clear();
            m_retryCount = 0;
            emit entryFinished(mascotId, false, {},
                QStringLiteral("mascotstore.download.canceled"),
                QStringLiteral("The download was canceled"));
        }
    }
}

bool MascotStoreCoordinator::isDownloading(QString const& mascotId) const {
    return mascotId == m_activeDownloadId;
}

bool MascotStoreCoordinator::isInstalling(QString const& mascotId) const {
    return mascotId == m_activeInstallId;
}

bool MascotStoreCoordinator::hasActiveOperation() const {
    return !m_activeDownloadId.isEmpty() || !m_activeInstallId.isEmpty();
}

void MascotStoreCoordinator::onDownloadProgress(QString destinationPath,
    qint64 received, qint64 total)
{
    QString mascotId = cacheKeyFor(destinationPath);
    if (!mascotId.isEmpty()) {
        emit entryProgress(mascotId, received, total);
    }
}

void MascotStoreCoordinator::onDownloadFinished(QString destinationPath,
    bool ok, QString errorCode, QString error)
{
    QString mascotId = m_activeDownloadId;
    if (mascotId.isEmpty()) {
        mascotId = cacheKeyFor(destinationPath);
    }
    if (!ok) {
        bool transient = errorCode == QStringLiteral("mascotstore.network") ||
            errorCode == QStringLiteral("mascotstore.timeout");
        if (transient && m_retryCount < kMaxDownloadRetries) {
            ++m_retryCount;
            QTimer::singleShot(1500, this,
                [this, mascotId, errorCode, error]() {
                // Keep the active id while waiting so the UI cannot start a
                // second package operation during the retry backoff. A
                // cancellation clears the id and invalidates this callback.
                if (m_activeDownloadId != mascotId) {
                    return;
                }
                MascotStoreIndex index;
                // Retry from the last known good index entry.
                MascotStoreCache::CachedIndex cached;
                QString parseError;
                if (m_cache->loadIndex(&cached) &&
                    index.parse(cached.body, &parseError))
                {
                    if (auto const* entry = index.findById(mascotId)) {
                        m_activeDownloadId.clear();
                        m_activeDownloadPath.clear();
                        downloadAndInstall(*entry);
                        return;
                    }
                }
                m_activeDownloadId.clear();
                m_activeDownloadPath.clear();
                emit entryFinished(mascotId, false, {}, errorCode, error);
            });
            return;
        }
        m_activeDownloadId.clear();
        m_activeDownloadPath.clear();
        emit entryFinished(mascotId, false, {}, errorCode, error);
        return;
    }

    m_activeDownloadId.clear();
    m_activeDownloadPath.clear();

    // Install off the GUI thread; storage writes must not block the UI.
    QString packagePath = destinationPath;
    QString storagePath = m_mascotStoragePath;
    m_activeInstallId = mascotId;
    emit entryInstallStarted(mascotId);
    auto future = QtConcurrent::run([packagePath, storagePath]() {
        QString installedName;
        QString error;
        bool ok = MascotPackage::installPackage(packagePath, storagePath,
            installedName, error);
        return std::tuple<bool, QString, QString> { ok, installedName, error };
    });
    auto *watcher = new QFutureWatcher<
        std::tuple<bool, QString, QString>>(this);
    connect(watcher, &QFutureWatcher<std::tuple<bool, QString, QString>>::finished,
        this, [this, watcher, mascotId]() {
            auto result = watcher->result();
            watcher->deleteLater();
            onInstallFinished(mascotId, std::get<0>(result),
                std::get<1>(result), {}, std::get<2>(result));
        });
    watcher->setFuture(future);
}

void MascotStoreCoordinator::onInstallFinished(QString mascotId, bool ok,
    QString installedName, QString errorCode, QString error)
{
    if (mascotId == m_activeInstallId) {
        m_activeInstallId.clear();
    }
    if (ok) {
        APP_LOG_INFO("mascotstore") << "Installed mascot id=\""
            << mascotId.toStdString() << "\" name=\""
            << installedName.toStdString() << "\"";
    }
    else {
        APP_LOG_ERROR("mascotstore") << "Install failed id=\""
            << mascotId.toStdString() << "\" error=\""
            << error.toStdString() << "\"";
    }
    emit entryFinished(mascotId, ok, installedName, errorCode, error);
}

QString MascotStoreCoordinator::cacheKeyFor(QString const& destinationPath) const {
    if (!m_activeDownloadId.isEmpty() &&
        destinationPath == m_activeDownloadPath)
    {
        return m_activeDownloadId;
    }
    return {};
}
