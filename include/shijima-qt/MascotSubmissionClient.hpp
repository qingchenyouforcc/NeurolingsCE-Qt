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

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class MascotSubmissionClient : public QObject
{
    Q_OBJECT
public:
    struct SubmissionResult {
        bool ok = false;
        QString id;
        QString status;
        QUrl prUrl;
        int prNumber = 0;
        QString errorCode;
        QString error;
    };

    explicit MascotSubmissionClient(QUrl serviceBaseUrl,
        QObject *parent = nullptr);
    ~MascotSubmissionClient() override;

    // Keeps the GitHub user access token only in memory for the duration of
    // the auth exchange; it is never stored, logged, or sent with the upload.
    void setAccessToken(QString const& accessToken);
    void clearSession();
    // Two-phase upload: exchanges the GitHub user token for a short-lived
    // submission session token at POST /v1/auth/github, then uploads with
    // only the session token.
    void submit(QString const& packagePath, QByteArray const& metadataJson,
        QString const& idempotencyKey, int timeoutMs = 300000);
    void cancel();
    bool isBusy() const;
    SubmissionResult lastResult() const;
    QString sessionToken() const;

signals:
    void uploadProgress(qint64 sent, qint64 total);
    void submissionFinished(MascotSubmissionClient::SubmissionResult result);

private slots:
    void onAuthFinished();
    void onFinished();

private:
    void startAuth();
    void startUpload(QString const& packagePath,
        QByteArray const& metadataJson, QString const& idempotencyKey,
        int timeoutMs);
    void emitFailure(QString const& code, QString const& message);

    QUrl m_serviceBaseUrl;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QString m_accessToken;
    QString m_sessionToken;
    QString m_pendingPackagePath;
    QByteArray m_pendingMetadataJson;
    QString m_pendingIdempotencyKey;
    int m_pendingTimeoutMs = 300000;
    SubmissionResult m_lastResult;
};
