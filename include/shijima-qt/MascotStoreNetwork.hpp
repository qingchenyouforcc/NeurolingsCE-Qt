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
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

struct MascotStoreIndexResponse {
    bool ok = false;
    bool notModified = false;
    QByteArray body;
    QString etag;
    QString lastModified;
    QString errorCode;
    QString error;
};

class MascotStoreNetwork : public QObject
{
    Q_OBJECT
public:
    explicit MascotStoreNetwork(QObject *parent = nullptr);
    ~MascotStoreNetwork() override;

    void fetchIndex(QUrl const& url, QString const& etag,
        QString const& lastModified, int timeoutMs = 15000);
    // Downloads to destinationPath + ".part", then renames it to
    // destinationPath after optional SHA-256 verification (run off the GUI
    // thread). A previous destination file is replaced only on success.
    void download(QUrl const& url, QString const& destinationPath,
        QString const& expectedSha256, int timeoutMs = 60000);
    // Cancel only the active package download. An index refresh can continue
    // independently; cancelAll() is reserved for teardown.
    void cancelDownload();
    void cancelAll();

signals:
    void indexFetched(MascotStoreIndexResponse response);
    void downloadProgress(QString destinationPath, qint64 received, qint64 total);
    void downloadFinished(QString destinationPath, bool ok,
        QString errorCode, QString error);

private slots:
    void onIndexFinished();
    void onDownloadReadyRead();
    void onDownloadFinished();

private:
    QString describeReplyFailure(QNetworkReply *reply) const;
    void cancelIndex();
    void abortDownload(bool notify, QString errorCode = {}, QString error = {});

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_indexReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_partialDownloadPath;
    QString m_downloadTargetPath;
    QString m_expectedSha256;
    qint64 m_downloadReceived = 0;
    qint64 m_downloadTotal = 0;
    quint64 m_downloadGeneration = 0;
};
