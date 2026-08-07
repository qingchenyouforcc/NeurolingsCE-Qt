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

#include "shijima-qt/MascotStoreNetwork.hpp"

#include "shijima-qt/AppLog.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtConcurrent>

namespace {

QString const kErrorNetwork = QStringLiteral("mascotstore.network");
QString const kErrorTimeout = QStringLiteral("mascotstore.timeout");
QString const kErrorHttp = QStringLiteral("mascotstore.http");
QString const kErrorSha256 = QStringLiteral("mascotstore.download.sha256_mismatch");
QString const kErrorWrite = QStringLiteral("mascotstore.download.write");
QString const kErrorCanceled = QStringLiteral("mascotstore.download.canceled");

QNetworkRequest makeRequest(QUrl const& url, QString const& etag,
    QString const& lastModified)
{
    QNetworkRequest request { url };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/") + QStringLiteral(NEUROLINGSCE_VERSION));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    if (!etag.isEmpty()) {
        request.setRawHeader("If-None-Match", etag.toUtf8());
    }
    if (!lastModified.isEmpty()) {
        request.setRawHeader("If-Modified-Since", lastModified.toUtf8());
    }
    return request;
}

}  // namespace

MascotStoreNetwork::MascotStoreNetwork(QObject *parent):
    QObject(parent),
    m_network(new QNetworkAccessManager(this))
{
}

MascotStoreNetwork::~MascotStoreNetwork() {
    cancelAll();
}

void MascotStoreNetwork::fetchIndex(QUrl const& url, QString const& etag,
    QString const& lastModified, int timeoutMs)
{
    if (m_indexReply != nullptr) {
        m_indexReply->abort();
        m_indexReply->deleteLater();
        m_indexReply = nullptr;
    }
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") &&
        url.scheme() != QStringLiteral("http")))
    {
        MascotStoreIndexResponse response;
        response.errorCode = kErrorNetwork;
        response.error = QStringLiteral("The store index URL is invalid");
        emit indexFetched(response);
        return;
    }
    APP_LOG_INFO("mascotstore") << "Fetching mascot index url=\""
        << url.toDisplayString().toStdString() << "\"";
    m_indexReply = m_network->get(makeRequest(url, etag, lastModified));
    connect(m_indexReply, &QNetworkReply::finished,
        this, &MascotStoreNetwork::onIndexFinished);
    QTimer::singleShot(timeoutMs, this, [this]() {
        if (m_indexReply != nullptr) {
            APP_LOG_WARN("mascotstore") << "Index fetch timed out";
            m_indexReply->abort();
        }
    });
}

void MascotStoreNetwork::onIndexFinished() {
    QNetworkReply *reply = m_indexReply;
    if (reply == nullptr) {
        return;
    }
    m_indexReply = nullptr;
    reply->deleteLater();

    MascotStoreIndexResponse response;
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        response.errorCode = kErrorTimeout;
        response.error = QStringLiteral("The store index request timed out");
        emit indexFetched(response);
        return;
    }
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 304) {
        response.ok = true;
        response.notModified = true;
        emit indexFetched(response);
        return;
    }
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        response.errorCode = status >= 400 ? kErrorHttp : kErrorNetwork;
        response.error = describeReplyFailure(reply);
        APP_LOG_WARN("mascotstore") << "Index fetch failed status=" << status
            << " error=" << response.error.toStdString();
        emit indexFetched(response);
        return;
    }
    response.ok = true;
    response.body = reply->readAll();
    response.etag = QString::fromUtf8(reply->rawHeader("ETag"));
    response.lastModified = QString::fromUtf8(
        reply->rawHeader("Last-Modified"));
    APP_LOG_INFO("mascotstore") << "Index fetch succeeded bytes="
        << response.body.size();
    emit indexFetched(response);
}

void MascotStoreNetwork::download(QUrl const& url, QString const& destinationPath,
    QString const& expectedSha256, int timeoutMs)
{
    cancelAll();
    QDir dir(QFileInfo(destinationPath).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit downloadFinished(destinationPath, false, kErrorWrite,
            QStringLiteral("Could not create the download directory"));
        return;
    }
    m_downloadTargetPath = destinationPath;
    m_partialDownloadPath = destinationPath + QStringLiteral(".part");
    m_expectedSha256 = expectedSha256;
    m_downloadReceived = 0;
    m_downloadTotal = 0;
    QFile::remove(m_partialDownloadPath);
    m_downloadFile = new QFile(m_partialDownloadPath, this);
    if (!m_downloadFile->open(QFile::WriteOnly | QFile::Truncate)) {
        QString error = m_downloadFile->errorString();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        emit downloadFinished(destinationPath, false, kErrorWrite, error);
        return;
    }
    QNetworkRequest request { url };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/") + QStringLiteral(NEUROLINGSCE_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    APP_LOG_INFO("mascotstore") << "Downloading mascot url=\""
        << url.toDisplayString().toStdString() << "\" target=\""
        << m_partialDownloadPath.toStdString() << "\"";
    m_downloadReply = m_network->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead,
        this, &MascotStoreNetwork::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished,
        this, &MascotStoreNetwork::onDownloadFinished);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            m_downloadReceived = received;
            m_downloadTotal = total;
            emit downloadProgress(m_downloadTargetPath, received, total);
        });
    QTimer::singleShot(timeoutMs, this, [this]() {
        if (m_downloadReply != nullptr) {
            APP_LOG_WARN("mascotstore") << "Download timed out target=\""
                << m_downloadTargetPath.toStdString() << "\"";
            m_downloadReply->abort();
        }
    });
}

void MascotStoreNetwork::onDownloadReadyRead() {
    if (m_downloadReply == nullptr || m_downloadFile == nullptr) {
        return;
    }
    QByteArray bytes = m_downloadReply->readAll();
    if (m_downloadFile->write(bytes) != bytes.size()) {
        emit downloadFinished(m_downloadTargetPath, false, kErrorWrite,
            m_downloadFile->errorString());
        m_downloadReply->abort();
    }
}

void MascotStoreNetwork::onDownloadFinished() {
    QNetworkReply *reply = m_downloadReply;
    if (reply == nullptr) {
        return;
    }
    m_downloadReply = nullptr;
    reply->deleteLater();
    if (m_downloadFile != nullptr) {
        m_downloadFile->flush();
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    QString destinationPath = m_downloadTargetPath;
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        QFile::remove(m_partialDownloadPath);
        emit downloadFinished(destinationPath, false, kErrorCanceled,
            QStringLiteral("The download was canceled"));
        return;
    }
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        QFile::remove(m_partialDownloadPath);
        emit downloadFinished(destinationPath, false,
            status >= 400 ? kErrorHttp : kErrorNetwork,
            describeReplyFailure(reply));
        return;
    }
    if (m_expectedSha256.isEmpty()) {
        QFile::remove(destinationPath);
        if (!QFile::rename(m_partialDownloadPath, destinationPath)) {
            emit downloadFinished(destinationPath, false, kErrorWrite,
                QStringLiteral("Could not finalize the downloaded file"));
            return;
        }
        emit downloadFinished(destinationPath, true, {}, {});
        return;
    }
    // Verify SHA-256 off the GUI thread, then finalize on the GUI thread.
    QString partialPath = m_partialDownloadPath;
    QString expected = m_expectedSha256.toLower();
    auto future = QtConcurrent::run([partialPath, expected]() {
        QFile file(partialPath);
        if (!file.open(QFile::ReadOnly)) {
            return QPair<bool, QString> { false,
                QStringLiteral("Could not read the downloaded file") };
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd()) {
            QByteArray chunk = file.read(1024 * 1024);
            if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
                return QPair<bool, QString> { false,
                    QStringLiteral("Could not read the downloaded file") };
            }
            hash.addData(chunk);
        }
        QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
        return QPair<bool, QString> { actual == expected,
            QStringLiteral("SHA-256 mismatch: expected %1, got %2")
                .arg(expected, actual) };
    });
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
        [this, watcher, destinationPath, partialPath]() {
            QPair<bool, QString> result = watcher->result();
            watcher->deleteLater();
            if (!result.first) {
                QFile::remove(partialPath);
                emit downloadFinished(destinationPath, false, kErrorSha256,
                    result.second);
                return;
            }
            QFile::remove(destinationPath);
            if (!QFile::rename(partialPath, destinationPath)) {
                emit downloadFinished(destinationPath, false, kErrorWrite,
                    QStringLiteral("Could not finalize the downloaded file"));
                return;
            }
            APP_LOG_INFO("mascotstore") << "Download verified target=\""
                << destinationPath.toStdString() << "\"";
            emit downloadFinished(destinationPath, true, {}, {});
        });
    watcher->setFuture(future);
}

void MascotStoreNetwork::cancelAll() {
    if (m_indexReply != nullptr) {
        QNetworkReply *reply = m_indexReply;
        reply->abort();  // may synchronously emit finished() and null the member
        if (m_indexReply == reply) {
            m_indexReply = nullptr;
            reply->deleteLater();
        }
    }
    if (m_downloadReply != nullptr) {
        QNetworkReply *reply = m_downloadReply;
        reply->abort();  // may synchronously emit finished() and null the member
        if (m_downloadReply == reply) {
            m_downloadReply = nullptr;
            reply->deleteLater();
        }
    }
    if (m_downloadFile != nullptr) {
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (!m_partialDownloadPath.isEmpty()) {
        QFile::remove(m_partialDownloadPath);
        m_partialDownloadPath.clear();
    }
    m_downloadTargetPath.clear();
}

QString MascotStoreNetwork::describeReplyFailure(QNetworkReply *reply) const {
    QString code = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toString();
    if (!code.isEmpty()) {
        return QStringLiteral("The server returned HTTP %1").arg(code);
    }
    QString detail = reply->errorString();
    return detail.isEmpty()
        ? QStringLiteral("The network request failed")
        : detail;
}
