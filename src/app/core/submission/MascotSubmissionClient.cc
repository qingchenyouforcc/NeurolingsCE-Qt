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

#include "shijima-qt/MascotSubmissionClient.hpp"

#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/Secrets.hpp"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

MascotSubmissionClient::MascotSubmissionClient(QUrl serviceBaseUrl,
    QObject *parent):
    QObject(parent),
    m_serviceBaseUrl(serviceBaseUrl),
    m_network(new QNetworkAccessManager(this))
{
}

MascotSubmissionClient::~MascotSubmissionClient() {
    cancel();
    m_accessToken.clear();
    m_sessionToken.clear();
}

void MascotSubmissionClient::submit(QString const& packagePath,
    QByteArray const& metadataJson, QString const& idempotencyKey,
    int timeoutMs)
{
    cancel();
    m_lastResult = SubmissionResult {};
    QFileInfo info(packagePath);
    if (!info.exists() || !info.isFile()) {
        emitFailure(QStringLiteral("submission.file_missing"),
            QStringLiteral("The selected mascot package does not exist"));
        return;
    }
    if (!m_serviceBaseUrl.isValid() ||
        m_serviceBaseUrl.scheme() != QStringLiteral("https"))
    {
        emitFailure(QStringLiteral("submission.not_configured"),
            QStringLiteral(
                "The submission service is not configured by the maintainer"));
        return;
    }
    if (!m_sessionToken.isEmpty()) {
        startUpload(packagePath, metadataJson, idempotencyKey, timeoutMs);
        return;
    }
    if (m_accessToken.isEmpty()) {
        emitFailure(QStringLiteral("submission.not_signed_in"),
            QStringLiteral("Sign in with GitHub before submitting a mascot"));
        return;
    }
    m_pendingPackagePath = packagePath;
    m_pendingMetadataJson = metadataJson;
    m_pendingIdempotencyKey = idempotencyKey;
    m_pendingTimeoutMs = timeoutMs;
    startAuth();
}

void MascotSubmissionClient::setAccessToken(QString const& accessToken) {
    m_accessToken = accessToken;
}

void MascotSubmissionClient::clearSession() {
    m_accessToken.clear();
    m_sessionToken.clear();
}

QString MascotSubmissionClient::sessionToken() const {
    return m_sessionToken;
}

void MascotSubmissionClient::startAuth() {
    QNetworkRequest request { m_serviceBaseUrl.resolved(QUrl {
        QStringLiteral("v1/auth/github") }) };
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
        QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->post(request, QByteArray());
    connect(m_reply, &QNetworkReply::finished,
        this, &MascotSubmissionClient::onAuthFinished);
    QTimer::singleShot(60000, this, [this]() {
        if (m_reply != nullptr) {
            m_reply->abort();
        }
    });
}

void MascotSubmissionClient::onAuthFinished() {
    QNetworkReply *reply = m_reply;
    if (reply == nullptr) {
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();
    QByteArray body = reply->readAll();
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    QJsonObject object;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        object = document.object();
    }
    if (reply->error() != QNetworkReply::NoError ||
        (status < 200 || status >= 300))
    {
        QJsonObject errorObject = object.value(
            QStringLiteral("error")).toObject();
        emitFailure(
            errorObject.value(QStringLiteral("code")).toString(
                QStringLiteral("submission.auth_failed")),
            errorObject.value(QStringLiteral("message")).toString());
        return;
    }
    QString token = object.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        emitFailure(QStringLiteral("submission.auth_invalid"),
            QStringLiteral("The submission service returned no session token"));
        return;
    }
    m_sessionToken = token;
    APP_LOG_INFO("submission") << "Obtained short-lived submission session token";
    startUpload(m_pendingPackagePath, m_pendingMetadataJson,
        m_pendingIdempotencyKey, m_pendingTimeoutMs);
}

void MascotSubmissionClient::startUpload(QString const& packagePath,
    QByteArray const& metadataJson, QString const& idempotencyKey,
    int timeoutMs)
{
    QFileInfo info(packagePath);
    QFile *file = new QFile(packagePath, this);
    if (!file->open(QFile::ReadOnly)) {
        QString fileError = file->errorString();
        delete file;
        emitFailure(QStringLiteral("submission.file_unreadable"), fileError);
        return;
    }
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    QHttpPart metadataPart;
    metadataPart.setHeader(QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json"));
    metadataPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"metadata\""));
    metadataPart.setBody(metadataJson);
    multiPart->append(metadataPart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
            .arg(info.fileName()));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest request { m_serviceBaseUrl.resolved(QUrl {
        QStringLiteral("v1/submissions") }) };
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
        QStringLiteral("Bearer %1").arg(m_sessionToken).toUtf8());
    if (!idempotencyKey.isEmpty()) {
        request.setRawHeader("X-Idempotency-Key", idempotencyKey.toUtf8());
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    APP_LOG_INFO("submission") << "Uploading mascot submission file=\""
        << info.fileName().toStdString() << "\"";
    m_reply = m_network->post(request, multiPart);
    multiPart->setParent(m_reply);
    connect(m_reply, &QNetworkReply::uploadProgress, this,
        [this](qint64 sent, qint64 total) {
            emit uploadProgress(sent, total);
        });
    connect(m_reply, &QNetworkReply::finished,
        this, &MascotSubmissionClient::onFinished);
    QTimer::singleShot(timeoutMs, this, [this]() {
        if (m_reply != nullptr) {
            m_reply->abort();
        }
    });
}

void MascotSubmissionClient::emitFailure(QString const& code,
    QString const& message)
{
    m_lastResult.errorCode = code;
    m_lastResult.error = message;
    APP_LOG_WARN("submission") << "Submission failed code="
        << code.toStdString()
        << " message=" << redactSensitiveText(message).toStdString();
    emit submissionFinished(m_lastResult);
}

void MascotSubmissionClient::onFinished() {
    QNetworkReply *reply = m_reply;
    if (reply == nullptr) {
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();
    QByteArray body = reply->readAll();
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emitFailure(QStringLiteral("submission.canceled"),
            QStringLiteral("The upload was canceled"));
        return;
    }
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    QJsonObject object;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        object = document.object();
    }
    if (reply->error() != QNetworkReply::NoError ||
        (status < 200 || status >= 300))
    {
        QJsonObject errorObject = object.value(
            QStringLiteral("error")).toObject();
        QString code = errorObject.value(QStringLiteral("code")).toString(
            status >= 400
                ? QStringLiteral("submission.service_error")
                : QStringLiteral("submission.network_error"));
        QString message = errorObject.value(
            QStringLiteral("message")).toString();
        if (message.isEmpty()) {
            message = reply->errorString();
        }
        emitFailure(code, message);
        return;
    }
    m_lastResult.ok = true;
    m_lastResult.id = object.value(QStringLiteral("id")).toString();
    m_lastResult.status = object.value(QStringLiteral("status")).toString();
    QJsonObject pr = object.value(QStringLiteral("pr")).toObject();
    m_lastResult.prNumber = pr.value(QStringLiteral("number")).toInt();
    m_lastResult.prUrl = QUrl { pr.value(QStringLiteral("url")).toString() };
    APP_LOG_INFO("submission") << "Submission accepted id=\""
        << m_lastResult.id.toStdString() << "\" status=\""
        << m_lastResult.status.toStdString() << "\"";
    emit submissionFinished(m_lastResult);
}

void MascotSubmissionClient::cancel() {
    if (m_reply != nullptr) {
        QNetworkReply *reply = m_reply;
        reply->abort();
        if (m_reply == reply) {
            m_reply = nullptr;
            reply->deleteLater();
        }
    }
}

bool MascotSubmissionClient::isBusy() const {
    return m_reply != nullptr;
}

MascotSubmissionClient::SubmissionResult MascotSubmissionClient::lastResult() const {
    return m_lastResult;
}
