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

#include <memory>

class CredentialStore;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QTimer;

class GitHubAuthManager : public QObject
{
    Q_OBJECT
public:
    enum class State {
        SignedOut,
        WaitingForDeviceCode,
        AwaitingAuthorization,
        SignedIn,
        Error,
    };
    Q_ENUM(State)

    struct UserInfo {
        QString login;
        QString userId;
        QString displayName;
        QUrl avatarUrl;
    };

    // Endpoints are injectable so tests can use a local mock server.
    explicit GitHubAuthManager(QString clientId,
        std::unique_ptr<CredentialStore> credentialStore,
        QUrl deviceCodeUrl = QUrl { QStringLiteral(
            "https://github.com/login/device/code") },
        QUrl accessTokenUrl = QUrl { QStringLiteral(
            "https://github.com/login/oauth/access_token") },
        QUrl apiBaseUrl = QUrl { QStringLiteral("https://api.github.com") },
        QObject *parent = nullptr);
    ~GitHubAuthManager() override;

    void startDeviceFlow();
    void cancel();
    void signOut();
    void refreshToken();
    // Tests use this to avoid opening a browser for loopback endpoints.
    void setAutoOpenVerificationUrl(bool enabled);

    State state() const;
    UserInfo userInfo() const;
    QString userCode() const;
    QUrl verificationUrl() const;
    QString lastError() const;
    QString lastErrorCode() const;
    // Access token for in-memory use (e.g. submission upload); never logged.
    QString accessToken() const;
    bool isSignedIn() const;
    bool canPersistLogin() const;

signals:
    void stateChanged();
    void deviceCodeReady(QString userCode, QUrl verificationUrl);
    void signedIn(GitHubAuthManager::UserInfo user);
    void signedOut();
    void errorOccurred(QString code, QString message);

private slots:
    void onDeviceCodeReply();
    void onPollReply();
    void onRefreshReply();
    void onUserReply();
    void startPolling();

private:
    void setState(State state);
    void clearPolling();
    void beginPolling(int intervalSeconds);
    void fetchUser();
    void persistTokens();
    bool loadStoredSession();
    QString describeReplyFailure(QNetworkReply *reply) const;
    QNetworkRequest makeFormRequest(QUrl const& url,
        QByteArray const& formBody) const;

    QString m_clientId;
    std::unique_ptr<CredentialStore> m_credentialStore;
    QUrl m_deviceCodeUrl;
    QUrl m_accessTokenUrl;
    QUrl m_apiBaseUrl;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_deviceReply = nullptr;
    QNetworkReply *m_pollReply = nullptr;
    QNetworkReply *m_refreshReply = nullptr;
    QNetworkReply *m_userReply = nullptr;
    QTimer *m_pollTimer = nullptr;

    State m_state = State::SignedOut;
    UserInfo m_userInfo;
    QString m_deviceCode;
    QString m_userCode;
    QUrl m_verificationUrl;
    QString m_accessToken;
    QString m_refreshToken;
    int m_pollIntervalSeconds = 5;
    QString m_lastError;
    QString m_lastErrorCode;
    bool m_autoOpenVerificationUrl = true;
};
