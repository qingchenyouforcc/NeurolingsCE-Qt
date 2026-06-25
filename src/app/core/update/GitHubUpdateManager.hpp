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

#include <QDateTime>
#include <QNetworkProxy>
#include <QObject>
#include <QStringList>
#include <QString>
#include <QUrl>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QSettings;

class GitHubUpdateManager : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Checking,
        UpToDate,
        UpdateAvailable,
        Downloading,
        ReadyToInstall,
        Error,
    };
    Q_ENUM(State)

    enum class AssetKind {
        None,
        Msi,
        Exe,
        Zip,
    };
    Q_ENUM(AssetKind)

    enum class CheckMode {
        Startup,
        Manual,
    };
    Q_ENUM(CheckMode)

    struct ReleaseAsset {
        AssetKind kind = AssetKind::None;
        QString name;
        QUrl url;
        QString sha256;
    };

    explicit GitHubUpdateManager(QSettings *settings, QObject *parent = nullptr);
    ~GitHubUpdateManager() override;

    void checkForUpdates(CheckMode mode = CheckMode::Manual);
    void downloadAndPrepareUpdate();
    void reloadNetworkSettings();
    void ignoreLatestVersion();
    void remindLater();
    void clearDownloadedInstaller();

    State state() const;
    AssetKind assetKind() const;
    QString currentVersion() const;
    QString latestVersion() const;
    QString releaseUrl() const;
    QString downloadedInstallerPath() const;
    QString lastError() const;
    QString statusText() const;
    QString detailText() const;
    QString publishedAtText() const;
    QString installButtonText() const;
    QString releaseButtonText() const;

    bool hasRelease() const;
    bool hasUpdate() const;
    bool isBusy() const;
    bool canCheckForUpdates() const;
    bool canDownloadInstaller() const;
    bool canInstallDownloadedUpdate() const;
    bool verifyDownloadedInstaller(QString &errorMessage) const;
    bool shouldShowIgnoreActions() const;

signals:
    void stateChanged();
    void startupUpdateAvailable(QString version);

private slots:
    void onCheckFinished();
    void onDownloadReadyRead();
    void onDownloadFinished();
    void onChecksumFinished();

private:
    enum class ProxyMode {
        Direct,
        System,
        Http,
        Socks5,
    };

    void resetLatestRelease();
    void resetDownloadState();
    void setState(State state, QString error = QString());
    void processLatestReleaseDocument(QByteArray const& bytes);
    void downloadChecksumManifest();
    bool verifyDownloadedInstallerWithManifest(QByteArray const& bytes, QString &errorMessage) const;
    QString describeReplyFailure(QNetworkReply *reply, QByteArray const& payload,
        QString const& context) const;
    QString tlsDiagnostics() const;
    QString payloadMessage(QByteArray const& payload) const;
    QString proxyDescription(QNetworkProxy const& proxy) const;
    QString normalizedVersionTag(QString const& tag) const;
    ProxyMode configuredProxyMode() const;
    QNetworkProxy proxyForUrl(QUrl const& url) const;
    QString updateCacheRoot() const;
    QString settingsIgnoredVersionKey() const;
    QString settingsLastCheckedAtKey() const;
    QString settingsDownloadedVersionKey() const;
    QString settingsDownloadedPathKey() const;
    QString settingsDownloadedSha256Key() const;
    QString settingsRemindVersionKey() const;
    QString settingsRemindAtKey() const;
    QString settingsProxyModeKey() const;
    QString settingsProxyHostKey() const;
    QString settingsProxyPortKey() const;
    QString settingsProxyUsernameKey() const;
    QString settingsProxyPasswordKey() const;
    bool hasPersistedInstallerForLatest() const;
    bool latestVersionIgnored() const;
    bool reminderSuppressed() const;
    void clearOutdatedSuppression();
    bool persistDownloadedInstaller(QString const& path);
    QString sha256ForFile(QString const& path, QString &errorMessage) const;
    void emitStartupSignalIfNeeded();

    QSettings *m_settings = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QNetworkReply *m_checksumReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_partialDownloadPath;
    QString m_downloadTargetPath;
    QString m_downloadedInstallerPath;
    QString m_downloadedInstallerSha256;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseUrl;
    QUrl m_checksumManifestUrl;
    QString m_lastError;
    QDateTime m_publishedAt;
    ReleaseAsset m_selectedAsset;
    State m_state = State::Idle;
    CheckMode m_lastCheckMode = CheckMode::Manual;
    qint64 m_downloadReceived = -1;
    qint64 m_downloadTotal = -1;
};
