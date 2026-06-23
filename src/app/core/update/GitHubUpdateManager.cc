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

#include "GitHubUpdateManager.hpp"
#include "shijima-qt/AppLog.hpp"

#include <exception>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QSslError>
#include <QSslSocket>
#include <QStandardPaths>
#include <QVersionNumber>

#ifndef NEUROLINGSCE_VERSION
#define NEUROLINGSCE_VERSION "0.1.0"
#endif

namespace {

QString const kLatestReleaseUrl =
    QStringLiteral("https://blog.qingchenyou.asia/NeurolingsCE/update/latest.json");

QString const kGitHubReleaseLatestUrl =
    QStringLiteral("https://github.com/qingchenyouforcc/NeurolingsCE/releases/latest");

QString formatVersion(QString const& version)
{
    return version.isEmpty() ? QString() : QStringLiteral("v%1").arg(version);
}

QString sanitize(QString const& value)
{
    QString text = value.simplified();
    if (text.size() > 220) {
        text = text.left(217) + QStringLiteral("...");
    }
    return text;
}

QVersionNumber versionNumberFor(QString const& version)
{
    return QVersionNumber::fromString(version);
}

bool isInstallerKind(GitHubUpdateManager::AssetKind kind)
{
    return kind == GitHubUpdateManager::AssetKind::Msi
        || kind == GitHubUpdateManager::AssetKind::Exe;
}

GitHubUpdateManager::ReleaseAsset selectBestWindowsAsset(QJsonArray const& assets,
    QUrl *checksumManifestUrl)
{
    GitHubUpdateManager::ReleaseAsset best;
    int bestRank = 99;

    for (auto const& assetValue : assets) {
        if (!assetValue.isObject()) {
            continue;
        }
        QJsonObject assetObject = assetValue.toObject();
        QString name = assetObject.value(QStringLiteral("name")).toString();
        QString url = assetObject.value(QStringLiteral("browser_download_url")).toString();
        if (name.isEmpty() || url.isEmpty()) {
            continue;
        }

        QString lowered = name.toLower();
        if ((lowered == QStringLiteral("sha256sums") ||
                lowered == QStringLiteral("sha256sums.txt")) &&
            checksumManifestUrl != nullptr)
        {
            *checksumManifestUrl = QUrl { url };
            continue;
        }
        GitHubUpdateManager::AssetKind kind = GitHubUpdateManager::AssetKind::None;
        int rank = 99;

        if (lowered.endsWith(QStringLiteral(".msi"))) {
            kind = GitHubUpdateManager::AssetKind::Msi;
            rank = 0;
        }
        else if (lowered.endsWith(QStringLiteral(".exe"))
            && lowered.contains(QStringLiteral("setup")))
        {
            kind = GitHubUpdateManager::AssetKind::Exe;
            rank = 1;
        }
        else if (lowered.endsWith(QStringLiteral(".zip"))
            && lowered.contains(QStringLiteral("windows"))
            && lowered.contains(QStringLiteral("x86_64")))
        {
            kind = GitHubUpdateManager::AssetKind::Zip;
            rank = 2;
        }

        if (rank < bestRank) {
            best.kind = kind;
            best.name = name;
            best.url = QUrl { url };
            best.sha256 = assetObject.value(QStringLiteral("sha256")).toString().trimmed().toLower();
            bestRank = rank;
        }
    }

    return best;
}

GitHubUpdateManager::ReleaseAsset selectBestWindowsAsset(QJsonObject const& assets,
    QUrl *checksumManifestUrl)
{
    QJsonArray releaseAssets;

    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        QJsonObject assetObject = it.value().toObject();
        QString name = assetObject.value(QStringLiteral("name")).toString();
        QString url = assetObject.value(QStringLiteral("url")).toString();
        if (name.isEmpty() || url.isEmpty()) {
            continue;
        }

        QJsonObject releaseAsset;
        releaseAsset.insert(QStringLiteral("name"), name);
        releaseAsset.insert(QStringLiteral("browser_download_url"), url);
        releaseAsset.insert(QStringLiteral("sha256"),
            assetObject.value(QStringLiteral("sha256")).toString());
        releaseAssets.append(releaseAsset);
    }

    if (checksumManifestUrl != nullptr) {
        QString checksumUrl = assets.value(QStringLiteral("sha256sums")).toObject()
            .value(QStringLiteral("url")).toString();
        if (!checksumUrl.isEmpty()) {
            *checksumManifestUrl = QUrl { checksumUrl };
        }
    }

    return selectBestWindowsAsset(releaseAssets, checksumManifestUrl);
}

}

GitHubUpdateManager::GitHubUpdateManager(QSettings *settings, QObject *parent):
    QObject(parent),
    m_settings(settings),
    m_network(new QNetworkAccessManager(this)),
    m_currentVersion(QStringLiteral(NEUROLINGSCE_VERSION))
{
    QString storedPath = m_settings->value(settingsDownloadedPathKey()).toString();
    if (!storedPath.isEmpty() && QFileInfo::exists(storedPath)) {
        m_downloadedInstallerPath = storedPath;
        m_downloadedInstallerSha256 =
            m_settings->value(settingsDownloadedSha256Key()).toString();
    }
    else {
        m_settings->remove(settingsDownloadedPathKey());
        m_settings->remove(settingsDownloadedVersionKey());
        m_settings->remove(settingsDownloadedSha256Key());
    }
    reloadNetworkSettings();
    APP_LOG_INFO("update") << "Initialized GitHub update manager"
        << " currentVersion=" << m_currentVersion.toStdString()
        << " tls=" << tlsDiagnostics().toStdString();
}

GitHubUpdateManager::~GitHubUpdateManager()
{
    resetDownloadState();
    if (m_checkReply != nullptr) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }
    if (m_checksumReply != nullptr) {
        m_checksumReply->abort();
        m_checksumReply->deleteLater();
        m_checksumReply = nullptr;
    }
}

void GitHubUpdateManager::checkForUpdates(CheckMode mode)
{
    if (!canCheckForUpdates()) {
        return;
    }

    if (m_checkReply != nullptr) {
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }

    m_lastCheckMode = mode;
    setState(State::Checking);
    QNetworkRequest request { QUrl { kLatestReleaseUrl } };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkProxy proxy = proxyForUrl(request.url());
    m_network->setProxy(proxy);
    APP_LOG_INFO("update") << "Checking for updates mode="
        << (mode == CheckMode::Startup ? "startup" : "manual")
        << " manifestUrl=" << kLatestReleaseUrl.toStdString()
        << " proxy=" << proxyDescription(proxy).toStdString();

    m_checkReply = m_network->get(request);
    connect(m_checkReply, &QNetworkReply::sslErrors, this,
        [this](QList<QSslError> const& errors) {
            QStringList descriptions;
            for (auto const& error : errors) {
                descriptions.append(error.errorString());
            }
            APP_LOG_WARN("update") << "SSL errors during update check: "
                << descriptions.join(QStringLiteral(" | ")).toStdString()
                << " tls=" << tlsDiagnostics().toStdString();
        });
    connect(m_checkReply, &QNetworkReply::finished,
        this, &GitHubUpdateManager::onCheckFinished);
}

void GitHubUpdateManager::downloadAndPrepareUpdate()
{
    if (!canDownloadInstaller()) {
        return;
    }

    resetDownloadState();

    QDir cacheDir(updateCacheRoot());
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
        setState(State::Error, tr("Could not create the update cache directory."));
        return;
    }

    QString versionDir = cacheDir.filePath(m_latestVersion);
    if (!QDir().mkpath(versionDir)) {
        setState(State::Error, tr("Could not create the versioned update cache directory."));
        return;
    }

    QString safeAssetName = QFileInfo(m_selectedAsset.name).fileName();
    if (safeAssetName.isEmpty() || safeAssetName != m_selectedAsset.name ||
        safeAssetName.contains(QLatin1Char('/')) ||
        safeAssetName.contains(QLatin1Char('\\')) ||
        safeAssetName.contains(QLatin1Char(':')))
    {
        setState(State::Error, tr("Unsafe update asset name."));
        return;
    }

    m_downloadTargetPath = QDir(versionDir).filePath(safeAssetName);
    m_partialDownloadPath = m_downloadTargetPath + QStringLiteral(".part");

    QFile::remove(m_partialDownloadPath);
    m_downloadFile = new QFile(m_partialDownloadPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
        setState(State::Error, tr("Could not open the update file for writing."));
        return;
    }

    QNetworkRequest request { m_selectedAsset.url };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/%1").arg(m_currentVersion));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkProxy proxy = proxyForUrl(request.url());
    m_network->setProxy(proxy);
    APP_LOG_INFO("update") << "Downloading installer asset="
        << m_selectedAsset.name.toStdString()
        << " url=" << m_selectedAsset.url.toString().toStdString()
        << " target=" << m_downloadTargetPath.toStdString()
        << " proxy=" << proxyDescription(proxy).toStdString();
    m_downloadReply = m_network->get(request);

    connect(m_downloadReply, &QNetworkReply::readyRead,
        this, &GitHubUpdateManager::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished,
        this, &GitHubUpdateManager::onDownloadFinished);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
        this, [this](qint64 received, qint64 total) {
            m_downloadReceived = received;
            m_downloadTotal = total;
            emit stateChanged();
        });
    connect(m_downloadReply, &QNetworkReply::sslErrors, this,
        [this](QList<QSslError> const& errors) {
            QStringList descriptions;
            for (auto const& error : errors) {
                descriptions.append(error.errorString());
            }
            APP_LOG_WARN("update") << "SSL errors during update download: "
                << descriptions.join(QStringLiteral(" | ")).toStdString()
                << " tls=" << tlsDiagnostics().toStdString();
        });

    setState(State::Downloading);
}

void GitHubUpdateManager::ignoreLatestVersion()
{
    if (!hasUpdate()) {
        return;
    }

    m_settings->setValue(settingsIgnoredVersionKey(), m_latestVersion);
    m_settings->remove(settingsRemindVersionKey());
    m_settings->remove(settingsRemindAtKey());
    APP_LOG_INFO("update") << "Ignored version " << m_latestVersion.toStdString();
    emit stateChanged();
}

void GitHubUpdateManager::remindLater()
{
    if (!hasUpdate()) {
        return;
    }

    QDateTime remindAt = QDateTime::currentDateTimeUtc().addDays(1);
    m_settings->setValue(settingsRemindVersionKey(), m_latestVersion);
    m_settings->setValue(settingsRemindAtKey(), remindAt.toString(Qt::ISODate));
    APP_LOG_INFO("update") << "Snoozed version " << m_latestVersion.toStdString()
        << " until " << remindAt.toString(Qt::ISODate).toStdString();
    emit stateChanged();
}

void GitHubUpdateManager::clearDownloadedInstaller()
{
    m_downloadedInstallerPath.clear();
    m_downloadedInstallerSha256.clear();
    m_settings->remove(settingsDownloadedPathKey());
    m_settings->remove(settingsDownloadedVersionKey());
    m_settings->remove(settingsDownloadedSha256Key());
}

void GitHubUpdateManager::reloadNetworkSettings()
{
    QNetworkProxy proxy = proxyForUrl(QUrl { kLatestReleaseUrl });
    m_network->setProxy(proxy);
    APP_LOG_INFO("update") << "Reloaded update proxy settings proxy="
        << proxyDescription(proxy).toStdString();
}

GitHubUpdateManager::State GitHubUpdateManager::state() const
{
    return m_state;
}

GitHubUpdateManager::AssetKind GitHubUpdateManager::assetKind() const
{
    return m_selectedAsset.kind;
}

QString GitHubUpdateManager::currentVersion() const
{
    return m_currentVersion;
}

QString GitHubUpdateManager::latestVersion() const
{
    return m_latestVersion;
}

QString GitHubUpdateManager::releaseUrl() const
{
    return m_releaseUrl;
}

QString GitHubUpdateManager::downloadedInstallerPath() const
{
    return m_downloadedInstallerPath;
}

QString GitHubUpdateManager::lastError() const
{
    return m_lastError;
}

QString GitHubUpdateManager::statusText() const
{
    switch (m_state) {
    case State::Idle:
        return tr("Check for the latest NeurolingsCE release.");
    case State::Checking:
        return tr("Checking for updates...");
    case State::UpToDate:
        return tr("You're up to date.");
    case State::UpdateAvailable:
        if (latestVersionIgnored()) {
            return tr("This version is currently ignored.");
        }
        if (reminderSuppressed()) {
            return tr("This update is snoozed for later.");
        }
        if (!isInstallerKind(m_selectedAsset.kind)) {
            return tr("A new version is available.");
        }
        return tr("A new version is ready to download.");
    case State::Downloading:
        return tr("Downloading update...");
    case State::ReadyToInstall:
        return tr("Update is ready to install.");
    case State::Error:
        return tr("Could not complete the update check.");
    }
    return QString();
}

QString GitHubUpdateManager::detailText() const
{
    switch (m_state) {
    case State::Idle:
        return tr("Current version: %1").arg(formatVersion(m_currentVersion));
    case State::Checking:
        return tr("Current version: %1").arg(formatVersion(m_currentVersion));
    case State::UpToDate:
        return tr("Current version: %1").arg(formatVersion(m_currentVersion));
    case State::UpdateAvailable:
        if (m_latestVersion.isEmpty()) {
            return QString();
        }
        if (!isInstallerKind(m_selectedAsset.kind)) {
            return tr("Latest stable release: %1. Open the release page to download it manually.")
                .arg(formatVersion(m_latestVersion));
        }
        return tr("Latest stable release: %1").arg(formatVersion(m_latestVersion));
    case State::Downloading:
        if (m_downloadReceived >= 0 && m_downloadTotal > 0) {
            return tr("Downloaded %1 of %2.")
                .arg(QLocale().formattedDataSize(m_downloadReceived))
                .arg(QLocale().formattedDataSize(m_downloadTotal));
        }
        if (m_downloadReceived >= 0) {
            return tr("Downloaded %1.")
                .arg(QLocale().formattedDataSize(m_downloadReceived));
        }
        return tr("Preparing download...");
    case State::ReadyToInstall:
        if (!m_downloadedInstallerPath.isEmpty()) {
            return QFileInfo(m_downloadedInstallerPath).fileName();
        }
        return tr("The installer has finished downloading.");
    case State::Error:
        return m_lastError;
    }
    return QString();
}

QString GitHubUpdateManager::publishedAtText() const
{
    if (!m_publishedAt.isValid()) {
        return QString();
    }

    return tr("Published: %1")
        .arg(QLocale().toString(m_publishedAt.toLocalTime(), QLocale::ShortFormat));
}

QString GitHubUpdateManager::installButtonText() const
{
    if (m_state == State::Downloading) {
        return tr("Downloading...");
    }
    if (m_state == State::ReadyToInstall) {
        return tr("Install Update");
    }
    return tr("Download and Install");
}

QString GitHubUpdateManager::releaseButtonText() const
{
    return tr("View Release Notes");
}

bool GitHubUpdateManager::hasRelease() const
{
    return !m_releaseUrl.isEmpty();
}

bool GitHubUpdateManager::hasUpdate() const
{
    if (m_latestVersion.isEmpty()) {
        return false;
    }
    return versionNumberFor(m_latestVersion) > versionNumberFor(m_currentVersion);
}

bool GitHubUpdateManager::isBusy() const
{
    return m_state == State::Checking || m_state == State::Downloading;
}

bool GitHubUpdateManager::canCheckForUpdates() const
{
    return !isBusy();
}

bool GitHubUpdateManager::canDownloadInstaller() const
{
    return m_state == State::UpdateAvailable && isInstallerKind(m_selectedAsset.kind);
}

bool GitHubUpdateManager::canInstallDownloadedUpdate() const
{
    return m_state == State::ReadyToInstall
        && isInstallerKind(m_selectedAsset.kind)
        && !m_downloadedInstallerPath.isEmpty()
        && !m_downloadedInstallerSha256.isEmpty()
        && QFileInfo::exists(m_downloadedInstallerPath);
}

bool GitHubUpdateManager::verifyDownloadedInstaller(QString &errorMessage) const
{
    if (!canInstallDownloadedUpdate()) {
        errorMessage = tr("No verified installer is ready to install.");
        return false;
    }
    if (m_selectedAsset.sha256.isEmpty()
        && (!m_checksumManifestUrl.isValid() || m_checksumManifestUrl.isEmpty()))
    {
        errorMessage = tr("The release did not publish SHA256SUMS.txt. Open the release page and install manually.");
        return false;
    }
    QString actualHash = sha256ForFile(m_downloadedInstallerPath, errorMessage);
    if (actualHash.isEmpty()) {
        return false;
    }
    if (actualHash != m_downloadedInstallerSha256) {
        errorMessage = tr("The downloaded installer changed after verification. Open the release page and install manually.");
        return false;
    }
    return true;
}

bool GitHubUpdateManager::shouldShowIgnoreActions() const
{
    return hasUpdate() && !latestVersionIgnored() && !reminderSuppressed();
}

void GitHubUpdateManager::onCheckFinished()
{
    if (m_checkReply == nullptr) {
        return;
    }

    m_settings->setValue(settingsLastCheckedAtKey(),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QByteArray payload = m_checkReply->readAll();
    QNetworkReply *reply = m_checkReply;
    m_checkReply = nullptr;
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    QString apiMessage = payloadMessage(payload);

    if (reply->error() != QNetworkReply::NoError) {
        QString error = describeReplyFailure(reply, payload, tr("Could not reach the update service."));
        APP_LOG_ERROR("update") << "Update check failed"
            << " networkError=" << static_cast<int>(reply->error())
            << " httpStatus=" << httpStatus
            << " reason=" << reason.toStdString()
            << " apiMessage=" << apiMessage.toStdString()
            << " errorString=" << reply->errorString().toStdString()
            << " tls=" << tlsDiagnostics().toStdString();
        setState(State::Error, error);
        reply->deleteLater();
        return;
    }

    if (httpStatus >= 400) {
        QString error = describeReplyFailure(reply, payload,
            tr("The update service returned HTTP status %1.").arg(httpStatus));
        APP_LOG_ERROR("update") << "Update check returned HTTP error"
            << " httpStatus=" << httpStatus
            << " reason=" << reason.toStdString()
            << " apiMessage=" << apiMessage.toStdString()
            << " tls=" << tlsDiagnostics().toStdString();
        setState(State::Error, error);
        reply->deleteLater();
        return;
    }

    try {
        processLatestReleaseDocument(payload);
        APP_LOG_INFO("update") << "Update check completed"
            << " latestVersion=" << m_latestVersion.toStdString()
            << " asset=" << m_selectedAsset.name.toStdString()
            << " hasUpdate=" << hasUpdate();
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("update") << "Exception while processing update response: " << ex.what();
        setState(State::Error, QString::fromUtf8(ex.what()));
    }

    reply->deleteLater();
}

void GitHubUpdateManager::onDownloadReadyRead()
{
    if (m_downloadReply == nullptr || m_downloadFile == nullptr) {
        return;
    }

    QByteArray bytes = m_downloadReply->readAll();
    if (bytes.isEmpty()) {
        return;
    }

    if (m_downloadFile->write(bytes) != bytes.size()) {
        APP_LOG_ERROR("update") << "Failed to write update chunk to disk target="
            << m_partialDownloadPath.toStdString();
        setState(State::Error, tr("Could not write the downloaded update to disk."));
        m_downloadReply->abort();
    }
}

void GitHubUpdateManager::onDownloadFinished()
{
    if (m_downloadReply == nullptr) {
        return;
    }

    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;

    if (m_downloadFile != nullptr) {
        m_downloadFile->flush();
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString error = describeReplyFailure(reply, reply->readAll(),
            tr("Could not download the installer."));
        APP_LOG_ERROR("update") << "Update download failed"
            << " networkError=" << static_cast<int>(reply->error())
            << " errorString=" << reply->errorString().toStdString()
            << " target=" << m_downloadTargetPath.toStdString()
            << " tls=" << tlsDiagnostics().toStdString();
        QFile::remove(m_partialDownloadPath);
        setState(State::Error, error);
        reply->deleteLater();
        return;
    }

    QFile::remove(m_downloadTargetPath);
    if (!QFile::rename(m_partialDownloadPath, m_downloadTargetPath)) {
        QFile::remove(m_partialDownloadPath);
        APP_LOG_ERROR("update") << "Failed to finalize installer download target="
            << m_downloadTargetPath.toStdString();
        setState(State::Error, tr("Could not finalize the downloaded installer."));
        reply->deleteLater();
        return;
    }

    APP_LOG_INFO("update") << "Installer download complete; verifying checksum path="
        << m_downloadTargetPath.toStdString();
    downloadChecksumManifest();
    reply->deleteLater();
}

void GitHubUpdateManager::downloadChecksumManifest()
{
    if (!m_selectedAsset.sha256.isEmpty()) {
        QString errorMessage;
        QString actualHash = sha256ForFile(m_downloadTargetPath, errorMessage);
        if (actualHash.isEmpty()) {
            QFile::remove(m_downloadTargetPath);
            setState(State::Error, errorMessage);
            return;
        }
        if (actualHash != m_selectedAsset.sha256) {
            QFile::remove(m_downloadTargetPath);
            setState(State::Error,
                tr("The downloaded installer failed SHA-256 verification. Open the release page and install manually."));
            return;
        }

        persistDownloadedInstaller(m_downloadTargetPath);
        APP_LOG_INFO("update") << "Installer checksum verified from update manifest path="
            << m_downloadTargetPath.toStdString();
        setState(State::ReadyToInstall);
        return;
    }

    if (!m_checksumManifestUrl.isValid() || m_checksumManifestUrl.isEmpty()) {
        QFile::remove(m_downloadTargetPath);
        setState(State::Error,
            tr("The release does not include SHA256SUMS.txt, so the installer was not started automatically. Open the release page and install manually."));
        return;
    }

    if (m_checksumReply != nullptr) {
        m_checksumReply->abort();
        m_checksumReply->deleteLater();
        m_checksumReply = nullptr;
    }

    QNetworkRequest request { m_checksumManifestUrl };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/%1").arg(m_currentVersion));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkProxy proxy = proxyForUrl(request.url());
    m_network->setProxy(proxy);
    APP_LOG_INFO("update") << "Downloading checksum manifest url="
        << m_checksumManifestUrl.toString().toStdString();
    m_checksumReply = m_network->get(request);
    connect(m_checksumReply, &QNetworkReply::finished,
        this, &GitHubUpdateManager::onChecksumFinished);
    connect(m_checksumReply, &QNetworkReply::sslErrors, this,
        [this](QList<QSslError> const& errors) {
            QStringList descriptions;
            for (auto const& error : errors) {
                descriptions.append(error.errorString());
            }
            APP_LOG_WARN("update") << "SSL errors during checksum download: "
                << descriptions.join(QStringLiteral(" | ")).toStdString()
                << " tls=" << tlsDiagnostics().toStdString();
        });
}

void GitHubUpdateManager::onChecksumFinished()
{
    if (m_checksumReply == nullptr) {
        return;
    }

    QNetworkReply *reply = m_checksumReply;
    m_checksumReply = nullptr;
    QByteArray payload = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        QString error = describeReplyFailure(reply, payload,
            tr("Could not download SHA256SUMS.txt."));
        APP_LOG_ERROR("update") << "Checksum manifest download failed"
            << " networkError=" << static_cast<int>(reply->error())
            << " errorString=" << reply->errorString().toStdString();
        QFile::remove(m_downloadTargetPath);
        setState(State::Error, error);
        reply->deleteLater();
        return;
    }

    QString errorMessage;
    if (!verifyDownloadedInstallerWithManifest(payload, errorMessage)) {
        QFile::remove(m_downloadTargetPath);
        APP_LOG_ERROR("update") << "Installer checksum verification failed: "
            << errorMessage.toStdString();
        setState(State::Error, errorMessage);
        reply->deleteLater();
        return;
    }

    persistDownloadedInstaller(m_downloadTargetPath);
    APP_LOG_INFO("update") << "Installer checksum verified path="
        << m_downloadTargetPath.toStdString();
    setState(State::ReadyToInstall);
    reply->deleteLater();
}

void GitHubUpdateManager::resetLatestRelease()
{
    m_latestVersion.clear();
    m_releaseUrl.clear();
    m_checksumManifestUrl = QUrl();
    m_lastError.clear();
    m_publishedAt = QDateTime();
    m_selectedAsset = ReleaseAsset {};
}

void GitHubUpdateManager::resetDownloadState()
{
    if (m_downloadReply != nullptr) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile != nullptr) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (!m_partialDownloadPath.isEmpty()) {
        QFile::remove(m_partialDownloadPath);
    }
    m_partialDownloadPath.clear();
    m_downloadTargetPath.clear();
    m_downloadReceived = -1;
    m_downloadTotal = -1;
}

void GitHubUpdateManager::setState(State state, QString error)
{
    m_state = state;
    m_lastError = error;
    if (state != State::Downloading) {
        m_downloadReceived = -1;
        m_downloadTotal = -1;
    }
    if (state == State::Error && !error.isEmpty()) {
        APP_LOG_ERROR("update") << "State set to error: " << error.toStdString();
    }
    emit stateChanged();
}

void GitHubUpdateManager::processLatestReleaseDocument(QByteArray const& bytes)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        APP_LOG_ERROR("update") << "Invalid update manifest parseError="
            << parseError.errorString().toStdString();
        setState(State::Error, tr("The update service returned an invalid response."));
        return;
    }

    QJsonObject manifestObject = document.object();
    QString version = normalizedVersionTag(
        manifestObject.value(QStringLiteral("version")).toString());
    if (version.isEmpty()) {
        version = normalizedVersionTag(
            manifestObject.value(QStringLiteral("tag")).toString());
    }
    if (version.isEmpty()) {
        APP_LOG_ERROR("update") << "Could not parse update manifest version"
            << " version=" << manifestObject.value(QStringLiteral("version")).toString().toStdString()
            << " tag=" << manifestObject.value(QStringLiteral("tag")).toString().toStdString();
        setState(State::Error, tr("The latest release version could not be parsed."));
        return;
    }

    resetLatestRelease();
    m_latestVersion = version;
    m_releaseUrl = manifestObject.value(QStringLiteral("release_page")).toString();
    if (m_releaseUrl.isEmpty()) {
        QString tag = manifestObject.value(QStringLiteral("tag")).toString().trimmed();
        m_releaseUrl = tag.isEmpty()
            ? kGitHubReleaseLatestUrl
            : QStringLiteral("https://github.com/qingchenyouforcc/NeurolingsCE/releases/tag/%1").arg(tag);
    }
    m_publishedAt = QDateTime::fromString(
        manifestObject.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
    m_selectedAsset = selectBestWindowsAsset(
        manifestObject.value(QStringLiteral("assets")).toObject(),
        &m_checksumManifestUrl);
    clearOutdatedSuppression();

    if (versionNumberFor(m_latestVersion) <= versionNumberFor(m_currentVersion)) {
        setState(State::UpToDate);
        return;
    }

    if (hasPersistedInstallerForLatest()) {
        setState(State::ReadyToInstall);
    }
    else {
        setState(State::UpdateAvailable);
    }

    emitStartupSignalIfNeeded();
}

bool GitHubUpdateManager::verifyDownloadedInstallerWithManifest(
    QByteArray const& bytes, QString &errorMessage) const
{
    if (m_downloadTargetPath.isEmpty() || m_selectedAsset.name.isEmpty()) {
        errorMessage = tr("No downloaded installer is available for checksum verification.");
        return false;
    }

    QString expectedHash;
    QString manifest = QString::fromUtf8(bytes);
    QStringList lines = manifest.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
            Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }
        QString hash = parts.first().trimmed().toLower();
        QString filename = parts.last().trimmed();
        if (filename.startsWith(QLatin1Char('*'))) {
            filename.remove(0, 1);
        }
        if (hash.size() == 64 && filename == m_selectedAsset.name) {
            expectedHash = hash;
            break;
        }
    }

    if (expectedHash.isEmpty()) {
        errorMessage = tr("SHA256SUMS.txt does not contain a hash for %1. The installer was not started automatically.")
            .arg(m_selectedAsset.name);
        return false;
    }

    QString actualHash = sha256ForFile(m_downloadTargetPath, errorMessage);
    if (actualHash.isEmpty()) {
        return false;
    }
    if (actualHash != expectedHash) {
        errorMessage = tr("The downloaded installer failed SHA-256 verification. Open the release page and install manually.");
        return false;
    }

    return true;
}

QString GitHubUpdateManager::describeReplyFailure(QNetworkReply *reply, QByteArray const& payload,
    QString const& context) const
{
    if (reply == nullptr) {
        return context;
    }

    QStringList parts;
    if (!context.isEmpty()) {
        parts.append(context);
    }

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString reason = sanitize(
        reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    QString apiMessage = payloadMessage(payload);
    QString errorString = sanitize(reply->errorString());

    if (httpStatus > 0) {
        if (!reason.isEmpty()) {
            parts.append(tr("HTTP %1: %2").arg(httpStatus).arg(reason));
        }
        else {
            parts.append(tr("HTTP %1").arg(httpStatus));
        }
    }

    if (!apiMessage.isEmpty()) {
        parts.append(apiMessage);
    }
    else if (!errorString.isEmpty()) {
        parts.append(errorString);
    }

    QString tls = tlsDiagnostics();
    if (!tls.isEmpty()) {
        parts.append(tls);
    }

    return parts.join(QStringLiteral("\n"));
}

QString GitHubUpdateManager::tlsDiagnostics() const
{
    QStringList parts;
    parts.append(tr("TLS backend: %1").arg(QSslSocket::activeBackend()));
    if (!QSslSocket::supportsSsl()) {
        parts.append(tr("SSL is unavailable in this build."));
    }
    QStringList backends = QSslSocket::availableBackends();
    if (!backends.isEmpty()) {
        parts.append(tr("Available backends: %1").arg(backends.join(QStringLiteral(", "))));
    }
    QString runtime = QSslSocket::sslLibraryVersionString();
    if (!runtime.isEmpty()) {
        parts.append(tr("Runtime SSL: %1").arg(runtime));
    }
    QString build = QSslSocket::sslLibraryBuildVersionString();
    if (!build.isEmpty()) {
        parts.append(tr("Build SSL: %1").arg(build));
    }
    return parts.join(QStringLiteral(" | "));
}

QString GitHubUpdateManager::payloadMessage(QByteArray const& payload) const
{
    if (payload.isEmpty()) {
        return QString();
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        QString message = sanitize(object.value(QStringLiteral("message")).toString());
        QString documentation = sanitize(
            object.value(QStringLiteral("documentation_url")).toString());
        if (!message.isEmpty() && !documentation.isEmpty()) {
            return QStringLiteral("%1 (%2)").arg(message, documentation);
        }
        if (!message.isEmpty()) {
            return message;
        }
    }

    return sanitize(QString::fromUtf8(payload));
}

QString GitHubUpdateManager::proxyDescription(QNetworkProxy const& proxy) const
{
    switch (proxy.type()) {
    case QNetworkProxy::NoProxy:
        return tr("Direct connection");
    case QNetworkProxy::DefaultProxy:
        return tr("System proxy");
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        if (!proxy.hostName().isEmpty()) {
            return tr("HTTP proxy %1:%2").arg(proxy.hostName()).arg(proxy.port());
        }
        return tr("HTTP proxy");
    case QNetworkProxy::Socks5Proxy:
        if (!proxy.hostName().isEmpty()) {
            return tr("SOCKS5 proxy %1:%2").arg(proxy.hostName()).arg(proxy.port());
        }
        return tr("SOCKS5 proxy");
    }
    return tr("Custom proxy");
}

QString GitHubUpdateManager::normalizedVersionTag(QString const& tag) const
{
    QString trimmed = tag.trimmed();
    if (trimmed.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        trimmed.remove(0, 1);
    }

    static QRegularExpression const versionRegex(
        QStringLiteral("(\\d+(?:\\.\\d+)+)"));
    auto match = versionRegex.match(trimmed);
    if (!match.hasMatch()) {
        return QString();
    }
    return match.captured(1);
}

GitHubUpdateManager::ProxyMode GitHubUpdateManager::configuredProxyMode() const
{
    QString mode = m_settings->value(settingsProxyModeKey(),
        QStringLiteral("system")).toString().trimmed().toLower();
    if (mode == QStringLiteral("direct")) {
        return ProxyMode::Direct;
    }
    if (mode == QStringLiteral("http")) {
        return ProxyMode::Http;
    }
    if (mode == QStringLiteral("socks5")) {
        return ProxyMode::Socks5;
    }
    return ProxyMode::System;
}

QNetworkProxy GitHubUpdateManager::proxyForUrl(QUrl const& url) const
{
    switch (configuredProxyMode()) {
    case ProxyMode::Direct:
        return QNetworkProxy(QNetworkProxy::NoProxy);
    case ProxyMode::Http:
    case ProxyMode::Socks5: {
        QString host = m_settings->value(settingsProxyHostKey()).toString().trimmed();
        quint16 port = static_cast<quint16>(
            m_settings->value(settingsProxyPortKey(), 8080).toUInt());
        QNetworkProxy proxy(configuredProxyMode() == ProxyMode::Http
                ? QNetworkProxy::HttpProxy
                : QNetworkProxy::Socks5Proxy,
            host,
            port,
            m_settings->value(settingsProxyUsernameKey()).toString(),
            m_settings->value(settingsProxyPasswordKey()).toString());
        if (host.isEmpty() || port == 0) {
            return QNetworkProxy(QNetworkProxy::NoProxy);
        }
        return proxy;
    }
    case ProxyMode::System: {
        QList<QNetworkProxy> proxies =
            QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
        if (!proxies.isEmpty()) {
            return proxies.first();
        }
        return QNetworkProxy(QNetworkProxy::DefaultProxy);
    }
    }

    return QNetworkProxy(QNetworkProxy::NoProxy);
}

QString GitHubUpdateManager::updateCacheRoot() const
{
    QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::cleanPath(dataRoot + QDir::separator() + QStringLiteral("updates"));
}

QString GitHubUpdateManager::settingsIgnoredVersionKey() const
{
    return QStringLiteral("update/ignoredVersion");
}

QString GitHubUpdateManager::settingsLastCheckedAtKey() const
{
    return QStringLiteral("update/lastCheckedAt");
}

QString GitHubUpdateManager::settingsDownloadedVersionKey() const
{
    return QStringLiteral("update/downloadedVersion");
}

QString GitHubUpdateManager::settingsDownloadedPathKey() const
{
    return QStringLiteral("update/downloadedInstallerPath");
}

QString GitHubUpdateManager::settingsDownloadedSha256Key() const
{
    return QStringLiteral("update/downloadedInstallerSha256");
}

QString GitHubUpdateManager::settingsRemindVersionKey() const
{
    return QStringLiteral("update/remindVersion");
}

QString GitHubUpdateManager::settingsRemindAtKey() const
{
    return QStringLiteral("update/remindAt");
}

QString GitHubUpdateManager::settingsProxyModeKey() const
{
    return QStringLiteral("update/proxyMode");
}

QString GitHubUpdateManager::settingsProxyHostKey() const
{
    return QStringLiteral("update/proxyHost");
}

QString GitHubUpdateManager::settingsProxyPortKey() const
{
    return QStringLiteral("update/proxyPort");
}

QString GitHubUpdateManager::settingsProxyUsernameKey() const
{
    return QStringLiteral("update/proxyUsername");
}

QString GitHubUpdateManager::settingsProxyPasswordKey() const
{
    return QStringLiteral("update/proxyPassword");
}

bool GitHubUpdateManager::hasPersistedInstallerForLatest() const
{
    QString storedVersion = m_settings->value(settingsDownloadedVersionKey()).toString();
    QString storedPath = m_settings->value(settingsDownloadedPathKey()).toString();
    QString storedSha256 = m_settings->value(settingsDownloadedSha256Key()).toString();
    if (storedVersion != m_latestVersion || storedPath.isEmpty() || storedSha256.isEmpty()) {
        return false;
    }
    if (!QFileInfo::exists(storedPath)) {
        return false;
    }
    return true;
}

bool GitHubUpdateManager::latestVersionIgnored() const
{
    return m_settings->value(settingsIgnoredVersionKey()).toString() == m_latestVersion;
}

bool GitHubUpdateManager::reminderSuppressed() const
{
    if (m_settings->value(settingsRemindVersionKey()).toString() != m_latestVersion) {
        return false;
    }

    QDateTime remindAt = QDateTime::fromString(
        m_settings->value(settingsRemindAtKey()).toString(), Qt::ISODate);
    return remindAt.isValid() && remindAt > QDateTime::currentDateTimeUtc();
}

void GitHubUpdateManager::clearOutdatedSuppression()
{
    if (m_settings->value(settingsIgnoredVersionKey()).toString() != m_latestVersion) {
        m_settings->remove(settingsIgnoredVersionKey());
    }

    if (m_settings->value(settingsRemindVersionKey()).toString() != m_latestVersion) {
        m_settings->remove(settingsRemindVersionKey());
        m_settings->remove(settingsRemindAtKey());
    }

    if (!hasPersistedInstallerForLatest()) {
        clearDownloadedInstaller();
    }
    else {
        m_downloadedInstallerPath = m_settings->value(settingsDownloadedPathKey()).toString();
        m_downloadedInstallerSha256 =
            m_settings->value(settingsDownloadedSha256Key()).toString();
    }
}

void GitHubUpdateManager::persistDownloadedInstaller(QString const& path)
{
    m_downloadedInstallerPath = path;
    QString error;
    m_downloadedInstallerSha256 = sha256ForFile(path, error);
    m_settings->setValue(settingsDownloadedVersionKey(), m_latestVersion);
    m_settings->setValue(settingsDownloadedPathKey(), path);
    m_settings->setValue(settingsDownloadedSha256Key(), m_downloadedInstallerSha256);
}

QString GitHubUpdateManager::sha256ForFile(QString const& path, QString &errorMessage) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = tr("Could not read the downloaded installer for checksum verification.");
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            errorMessage = tr("Could not read the downloaded installer for checksum verification.");
            return {};
        }
        hash.addData(chunk);
    }

    return QString::fromLatin1(hash.result().toHex()).toLower();
}

void GitHubUpdateManager::emitStartupSignalIfNeeded()
{
    if (m_lastCheckMode != CheckMode::Startup) {
        return;
    }
    if (!hasUpdate()) {
        return;
    }
    if (latestVersionIgnored() || reminderSuppressed()) {
        return;
    }

    emit startupUpdateAvailable(m_latestVersion);
}
