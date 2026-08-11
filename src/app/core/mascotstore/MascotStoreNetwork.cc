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
    cancelIndex();
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
    QNetworkReply *reply = m_network->get(makeRequest(url, etag, lastModified));
    m_indexReply = reply;
    connect(reply, &QNetworkReply::finished,
        this, &MascotStoreNetwork::onIndexFinished);
    QTimer::singleShot(timeoutMs, this, [this, reply]() {
        if (m_indexReply == reply) {
            APP_LOG_WARN("mascotstore") << "Index fetch timed out";
            reply->abort();
        }
    });
}

void MascotStoreNetwork::onIndexFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr || reply != m_indexReply) {
        if (reply != nullptr) {
            reply->deleteLater();
        }
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
    // Package downloads must not interrupt an index refresh. A previous
    // package transfer is superseded silently; the coordinator rejects a
    // second active entry, so this is only a defensive cleanup path.
    abortDownload(false);
    ++m_downloadGeneration;
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
    QNetworkReply *reply = m_network->get(request);
    m_downloadReply = reply;
    connect(reply, &QNetworkReply::readyRead,
        this, &MascotStoreNetwork::onDownloadReadyRead);
    connect(reply, &QNetworkReply::finished,
        this, &MascotStoreNetwork::onDownloadFinished);
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            m_downloadReceived = received;
            m_downloadTotal = total;
            emit downloadProgress(m_downloadTargetPath, received, total);
        });
    QTimer::singleShot(timeoutMs, this, [this, reply]() {
        if (m_downloadReply == reply) {
            APP_LOG_WARN("mascotstore") << "Download timed out target=\""
                << m_downloadTargetPath.toStdString() << "\"";
            abortDownload(true, kErrorTimeout,
                QStringLiteral("The download request timed out"));
        }
    });
}

void MascotStoreNetwork::onDownloadReadyRead() {
    if (m_downloadReply == nullptr || m_downloadFile == nullptr) {
        return;
    }
    QByteArray bytes = m_downloadReply->readAll();
    if (m_downloadFile->write(bytes) != bytes.size()) {
        abortDownload(true, kErrorWrite, m_downloadFile->errorString());
    }
}

void MascotStoreNetwork::onDownloadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr || reply != m_downloadReply) {
        if (reply != nullptr) {
            reply->deleteLater();
        }
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
        m_partialDownloadPath.clear();
        m_downloadTargetPath.clear();
        emit downloadFinished(destinationPath, false, kErrorCanceled,
            QStringLiteral("The download was canceled"));
        return;
    }
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        QFile::remove(m_partialDownloadPath);
        m_partialDownloadPath.clear();
        m_downloadTargetPath.clear();
        emit downloadFinished(destinationPath, false,
            status >= 400 ? kErrorHttp : kErrorNetwork,
            describeReplyFailure(reply));
        return;
    }
    if (m_expectedSha256.isEmpty()) {
        QFile::remove(destinationPath);
        if (!QFile::rename(m_partialDownloadPath, destinationPath)) {
            m_partialDownloadPath.clear();
            m_downloadTargetPath.clear();
            emit downloadFinished(destinationPath, false, kErrorWrite,
                QStringLiteral("Could not finalize the downloaded file"));
            return;
        }
        m_partialDownloadPath.clear();
        m_downloadTargetPath.clear();
        emit downloadFinished(destinationPath, true, {}, {});
        return;
    }
    // Verify SHA-256 off the GUI thread, then finalize on the GUI thread.
    QString partialPath = m_partialDownloadPath;
    QString expected = m_expectedSha256.toLower();
    quint64 generation = m_downloadGeneration;
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
        [this, watcher, destinationPath, partialPath, generation]() {
            QPair<bool, QString> result = watcher->result();
            watcher->deleteLater();
            if (generation != m_downloadGeneration) {
                return;
            }
            if (!result.first) {
                QFile::remove(partialPath);
                m_partialDownloadPath.clear();
                m_downloadTargetPath.clear();
                emit downloadFinished(destinationPath, false, kErrorSha256,
                    result.second);
                return;
            }
            QFile::remove(destinationPath);
            if (!QFile::rename(partialPath, destinationPath)) {
                m_partialDownloadPath.clear();
                m_downloadTargetPath.clear();
                emit downloadFinished(destinationPath, false, kErrorWrite,
                    QStringLiteral("Could not finalize the downloaded file"));
                return;
            }
            m_partialDownloadPath.clear();
            m_downloadTargetPath.clear();
            APP_LOG_INFO("mascotstore") << "Download verified target=\""
                << destinationPath.toStdString() << "\"";
            emit downloadFinished(destinationPath, true, {}, {});
        });
    watcher->setFuture(future);
}

void MascotStoreNetwork::cancelIndex() {
    if (m_indexReply != nullptr) {
        QNetworkReply *reply = m_indexReply;
        m_indexReply = nullptr;
        reply->abort();  // may synchronously emit finished() and null the member
        reply->deleteLater();
    }
}

void MascotStoreNetwork::abortDownload(bool notify, QString errorCode,
    QString error) {
    ++m_downloadGeneration;
    QString destinationPath = m_downloadTargetPath;
    bool hadDownload = m_downloadReply != nullptr || m_downloadFile != nullptr ||
        !m_partialDownloadPath.isEmpty();
    if (m_downloadReply != nullptr) {
        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;
        reply->abort();  // may synchronously emit finished() and null the member
        reply->deleteLater();
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
    if (notify && hadDownload && !destinationPath.isEmpty()) {
        if (errorCode.isEmpty()) {
            errorCode = kErrorCanceled;
        }
        if (error.isEmpty()) {
            error = QStringLiteral("The download was canceled");
        }
        emit downloadFinished(destinationPath, false,
            errorCode, error);
    }
}

void MascotStoreNetwork::cancelDownload() {
    abortDownload(true);
}

void MascotStoreNetwork::cancelAll() {
    cancelIndex();
    abortDownload(true);
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
