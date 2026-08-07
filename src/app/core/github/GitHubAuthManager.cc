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

#include "shijima-qt/GitHubAuthManager.hpp"

#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/CredentialStore.hpp"
#include "shijima-qt/Secrets.hpp"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <QDateTime>

namespace {

QString const kServiceName = QStringLiteral("NeurolingsCE-GitHub");
QString const kAccountName = QStringLiteral("oauth");

QByteArray urlEncodeForm(QUrlQuery const& query) {
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

}  // namespace

GitHubAuthManager::GitHubAuthManager(QString clientId,
    std::unique_ptr<CredentialStore> credentialStore,
    QUrl deviceCodeUrl, QUrl accessTokenUrl, QUrl apiBaseUrl, QObject *parent):
    QObject(parent),
    m_clientId(clientId),
    m_credentialStore(std::move(credentialStore)),
    m_deviceCodeUrl(deviceCodeUrl),
    m_accessTokenUrl(accessTokenUrl),
    m_apiBaseUrl(apiBaseUrl),
    m_network(new QNetworkAccessManager(this)),
    m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &GitHubAuthManager::startPolling);
    loadStoredSession();
}

GitHubAuthManager::~GitHubAuthManager() {
    cancel();
}

void GitHubAuthManager::startDeviceFlow() {
    cancel();
    if (m_clientId.isEmpty()) {
        m_lastErrorCode = QStringLiteral("github.not_configured");
        m_lastError = QStringLiteral(
            "GitHub login is not configured by the maintainer");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    // GitHub App Device Flow does not use the traditional OAuth scope model:
    // the Login App has no repository/org/user permissions and GET /user is
    // available without any scope. Sending only client_id keeps the request
    // minimal and avoids accidentally requesting write-capable scopes.
    QNetworkRequest request = makeFormRequest(m_deviceCodeUrl,
        urlEncodeForm(query));
    APP_LOG_INFO("github") << "Starting GitHub device flow client_id_set="
        << (!m_clientId.isEmpty() ? "1" : "0");
    m_deviceReply = m_network->post(request, urlEncodeForm(query));
    connect(m_deviceReply, &QNetworkReply::finished,
        this, &GitHubAuthManager::onDeviceCodeReply);
    setState(State::WaitingForDeviceCode);
}

void GitHubAuthManager::onDeviceCodeReply() {
    QNetworkReply *reply = m_deviceReply;
    if (reply == nullptr) {
        return;
    }
    m_deviceReply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_lastErrorCode = QStringLiteral("github.device_code_network");
        m_lastError = describeReplyFailure(reply);
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(
        reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject())
    {
        m_lastErrorCode = QStringLiteral("github.device_code_invalid");
        m_lastError = QStringLiteral("GitHub returned an invalid device code response");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QJsonObject object = document.object();
    QString errorCode = object.value(QStringLiteral("error")).toString();
    if (!errorCode.isEmpty()) {
        if (errorCode == QStringLiteral("device_flow_disabled")) {
            m_lastErrorCode = QStringLiteral("github.device_flow_disabled");
            m_lastError = QStringLiteral(
                "The GitHub App has Device Flow disabled; enable it in the "
                "app settings and try again");
            setState(State::Error);
            emit errorOccurred(m_lastErrorCode, m_lastError);
            return;
        }
        m_lastErrorCode = QStringLiteral("github.device_code_error");
        m_lastError = QStringLiteral("GitHub rejected the device code request");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    m_deviceCode = object.value(QStringLiteral("device_code")).toString();
    m_userCode = object.value(QStringLiteral("user_code")).toString();
    m_verificationUrl = QUrl { object.value(
        QStringLiteral("verification_uri")).toString() };
    m_pollIntervalSeconds = object.value(QStringLiteral("interval")).toInt(5);
    if (m_pollIntervalSeconds < 1) {
        m_pollIntervalSeconds = 5;
    }
    if (m_deviceCode.isEmpty() || m_userCode.isEmpty() ||
        !m_verificationUrl.isValid())
    {
        m_lastErrorCode = QStringLiteral("github.device_code_invalid");
        m_lastError = QStringLiteral("GitHub device code response is incomplete");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    setState(State::AwaitingAuthorization);
    emit deviceCodeReady(m_userCode, m_verificationUrl);
    if (m_autoOpenVerificationUrl) {
        QDesktopServices::openUrl(m_verificationUrl);
    }
    beginPolling(m_pollIntervalSeconds);
}

void GitHubAuthManager::startPolling() {
    if (m_deviceCode.isEmpty()) {
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("device_code"), m_deviceCode);
    query.addQueryItem(QStringLiteral("grant_type"),
        QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    QByteArray body = urlEncodeForm(query);
    QNetworkRequest request = makeFormRequest(m_accessTokenUrl, body);
    m_pollReply = m_network->post(request, body);
    connect(m_pollReply, &QNetworkReply::finished,
        this, &GitHubAuthManager::onPollReply);
}

void GitHubAuthManager::onPollReply() {
    QNetworkReply *reply = m_pollReply;
    if (reply == nullptr) {
        return;
    }
    m_pollReply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_lastErrorCode = QStringLiteral("github.poll_network");
        m_lastError = describeReplyFailure(reply);
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QByteArray body = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // GitHub may return form-encoded responses; try parsing as such.
        QUrlQuery form(QString::fromUtf8(body));
        QString access = form.queryItemValue(QStringLiteral("access_token"));
        if (!access.isEmpty()) {
            m_accessToken = access;
            m_refreshToken = form.queryItemValue(QStringLiteral("refresh_token"));
            persistTokens();
            fetchUser();
            return;
        }
        m_lastErrorCode = QStringLiteral("github.poll_invalid");
        m_lastError = QStringLiteral("GitHub polling response is invalid");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QJsonObject object = document.object();
    QString access = object.value(QStringLiteral("access_token")).toString();
    if (!access.isEmpty()) {
        m_accessToken = access;
        m_refreshToken = object.value(QStringLiteral("refresh_token")).toString();
        persistTokens();
        fetchUser();
        return;
    }
    QString errorCode = object.value(QStringLiteral("error")).toString();
    if (errorCode == QStringLiteral("authorization_pending")) {
        beginPolling(m_pollIntervalSeconds);
        return;
    }
    if (errorCode == QStringLiteral("slow_down")) {
        int suggested = object.value(QStringLiteral("interval")).toInt(0);
        m_pollIntervalSeconds = (suggested > m_pollIntervalSeconds)
            ? suggested
            : m_pollIntervalSeconds + 5;
        if (m_pollIntervalSeconds > 600) {
            m_pollIntervalSeconds = 600;
        }
        beginPolling(m_pollIntervalSeconds);
        return;
    }
    if (errorCode == QStringLiteral("device_flow_disabled")) {
        m_lastErrorCode = QStringLiteral("github.device_flow_disabled");
        m_lastError = QStringLiteral(
            "The GitHub App has Device Flow disabled; enable it in the app "
            "settings and try again");
        clearPolling();
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    if (errorCode == QStringLiteral("access_denied")) {
        m_lastErrorCode = QStringLiteral("github.access_denied");
        m_lastError = QStringLiteral("GitHub authorization was denied");
        clearPolling();
        setState(State::SignedOut);
        emit signedOut();
        return;
    }
    if (errorCode == QStringLiteral("expired_token") ||
        errorCode == QStringLiteral("incorrect_device_code"))
    {
        m_lastErrorCode = QStringLiteral("github.device_code_expired");
        m_lastError = QStringLiteral("The GitHub verification code expired; start again");
        clearPolling();
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    m_lastErrorCode = QStringLiteral("github.poll_failed");
    m_lastError = errorCode.isEmpty()
        ? QStringLiteral("GitHub polling failed")
        : errorCode;
    clearPolling();
    setState(State::Error);
    emit errorOccurred(m_lastErrorCode, m_lastError);
}

void GitHubAuthManager::fetchUser() {
    QNetworkRequest request { m_apiBaseUrl.resolved(QUrl {
        QStringLiteral("user") }) };
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("Authorization",
        QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    m_userReply = m_network->get(request);
    connect(m_userReply, &QNetworkReply::finished,
        this, &GitHubAuthManager::onUserReply);
}

void GitHubAuthManager::onUserReply() {
    QNetworkReply *reply = m_userReply;
    if (reply == nullptr) {
        return;
    }
    m_userReply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        // Token may be expired; try refreshing before giving up.
        m_lastErrorCode = QStringLiteral("github.user_fetch_failed");
        m_lastError = describeReplyFailure(reply);
        if (!m_refreshToken.isEmpty()) {
            refreshToken();
            return;
        }
        signOut();
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(
        reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject())
    {
        m_lastErrorCode = QStringLiteral("github.user_invalid");
        m_lastError = QStringLiteral("GitHub user response is invalid");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    QJsonObject object = document.object();
    m_userInfo.login = object.value(QStringLiteral("login")).toString();
    QJsonValue const idValue = object.value(QStringLiteral("id"));
    m_userInfo.userId = idValue.isDouble()
        ? QString::number(static_cast<qint64>(idValue.toDouble()))
        : idValue.toString();
    m_userInfo.displayName = object.value(QStringLiteral("name")).toString();
    m_userInfo.avatarUrl = QUrl { object.value(
        QStringLiteral("avatar_url")).toString() };
    if (m_userInfo.login.isEmpty() || m_userInfo.userId.isEmpty()) {
        m_lastErrorCode = QStringLiteral("github.user_invalid");
        m_lastError = QStringLiteral(
            "GitHub user response has no login or numeric user id");
        setState(State::Error);
        emit errorOccurred(m_lastErrorCode, m_lastError);
        return;
    }
    setState(State::SignedIn);
    emit signedIn(m_userInfo);
}

void GitHubAuthManager::refreshToken() {
    if (m_refreshToken.isEmpty()) {
        signOut();
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    query.addQueryItem(QStringLiteral("grant_type"),
        QStringLiteral("refresh_token"));
    QByteArray body = urlEncodeForm(query);
    QNetworkRequest request = makeFormRequest(m_accessTokenUrl, body);
    m_refreshReply = m_network->post(request, body);
    connect(m_refreshReply, &QNetworkReply::finished,
        this, &GitHubAuthManager::onRefreshReply);
}

void GitHubAuthManager::onRefreshReply() {
    QNetworkReply *reply = m_refreshReply;
    if (reply == nullptr) {
        return;
    }
    m_refreshReply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        signOut();
        return;
    }
    QByteArray body = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    QJsonObject object;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        object = document.object();
    }
    QString access = object.value(QStringLiteral("access_token")).toString();
    if (access.isEmpty()) {
        QUrlQuery form(QString::fromUtf8(body));
        access = form.queryItemValue(QStringLiteral("access_token"));
    }
    if (access.isEmpty()) {
        signOut();
        return;
    }
    m_accessToken = access;
    QString newRefresh = object.value(QStringLiteral("refresh_token")).toString();
    if (!newRefresh.isEmpty()) {
        m_refreshToken = newRefresh;
    }
    persistTokens();
    fetchUser();
}

void GitHubAuthManager::persistTokens() {
    if (m_credentialStore == nullptr ||
        !m_credentialStore->isAvailable())
    {
        return;
    }
    QJsonObject payload;
    payload[QStringLiteral("access_token")] = m_accessToken;
    payload[QStringLiteral("refresh_token")] = m_refreshToken;
    QString error;
    if (m_credentialStore->save(kServiceName, kAccountName,
            QString::fromUtf8(QJsonDocument(payload).toJson(
                QJsonDocument::Compact)), &error) != CredentialStore::Status::Ok)
    {
        APP_LOG_WARN("github") << "Could not persist GitHub session: "
            << redactSensitiveText(error).toStdString();
    }
}

bool GitHubAuthManager::loadStoredSession() {
    if (m_credentialStore == nullptr ||
        !m_credentialStore->isAvailable())
    {
        return false;
    }
    QString payload;
    if (m_credentialStore->load(kServiceName, kAccountName, &payload) !=
        CredentialStore::Status::Ok || payload.isEmpty())
    {
        return false;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(
        payload.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject())
    {
        return false;
    }
    QJsonObject object = document.object();
    m_accessToken = object.value(QStringLiteral("access_token")).toString();
    m_refreshToken = object.value(QStringLiteral("refresh_token")).toString();
    if (m_accessToken.isEmpty()) {
        return false;
    }
    setState(State::SignedIn);
    fetchUser();
    return true;
}

void GitHubAuthManager::cancel() {
    clearPolling();
    if (m_deviceReply != nullptr) {
        QNetworkReply *reply = m_deviceReply;
        reply->abort();
        if (m_deviceReply == reply) {
            m_deviceReply = nullptr;
            reply->deleteLater();
        }
    }
    if (m_pollReply != nullptr) {
        QNetworkReply *reply = m_pollReply;
        reply->abort();
        if (m_pollReply == reply) {
            m_pollReply = nullptr;
            reply->deleteLater();
        }
    }
    if (m_refreshReply != nullptr) {
        QNetworkReply *reply = m_refreshReply;
        reply->abort();
        if (m_refreshReply == reply) {
            m_refreshReply = nullptr;
            reply->deleteLater();
        }
    }
    if (m_userReply != nullptr) {
        QNetworkReply *reply = m_userReply;
        reply->abort();
        if (m_userReply == reply) {
            m_userReply = nullptr;
            reply->deleteLater();
        }
    }
}

void GitHubAuthManager::signOut() {
    cancel();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_userInfo = UserInfo {};
    m_deviceCode.clear();
    m_userCode.clear();
    m_verificationUrl.clear();
    if (m_credentialStore != nullptr) {
        QString error;
        m_credentialStore->removeAll(kServiceName, &error);
    }
    setState(State::SignedOut);
    emit signedOut();
}

void GitHubAuthManager::clearPolling() {
    m_pollTimer->stop();
    if (m_pollReply != nullptr) {
        QNetworkReply *reply = m_pollReply;
        reply->abort();
        if (m_pollReply == reply) {
            m_pollReply = nullptr;
            reply->deleteLater();
        }
    }
}

void GitHubAuthManager::beginPolling(int intervalSeconds) {
    m_pollTimer->start(intervalSeconds * 1000);
}

GitHubAuthManager::State GitHubAuthManager::state() const {
    return m_state;
}

GitHubAuthManager::UserInfo GitHubAuthManager::userInfo() const {
    return m_userInfo;
}

QString GitHubAuthManager::userCode() const {
    return m_userCode;
}

QUrl GitHubAuthManager::verificationUrl() const {
    return m_verificationUrl;
}

QString GitHubAuthManager::lastError() const {
    return m_lastError;
}

QString GitHubAuthManager::lastErrorCode() const {
    return m_lastErrorCode;
}

QString GitHubAuthManager::accessToken() const {
    return m_accessToken;
}

bool GitHubAuthManager::isSignedIn() const {
    return m_state == State::SignedIn;
}

bool GitHubAuthManager::canPersistLogin() const {
    return m_credentialStore != nullptr &&
        m_credentialStore->isAvailable();
}

void GitHubAuthManager::setAutoOpenVerificationUrl(bool enabled) {
    m_autoOpenVerificationUrl = enabled;
}

void GitHubAuthManager::setState(State state) {
    m_state = state;
    emit stateChanged();
}

QString GitHubAuthManager::describeReplyFailure(QNetworkReply *reply) const {
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0) {
        return QStringLiteral("GitHub returned HTTP %1").arg(status);
    }
    return reply->errorString().isEmpty()
        ? QStringLiteral("The GitHub request failed")
        : reply->errorString();
}

QNetworkRequest GitHubAuthManager::makeFormRequest(QUrl const& url,
    QByteArray const& formBody) const
{
    QNetworkRequest request { url };
    request.setHeader(QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("NeurolingsCE/") + QStringLiteral(NEUROLINGSCE_VERSION));
    request.setHeader(QNetworkRequest::ContentLengthHeader,
        static_cast<int>(formBody.size()));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}
