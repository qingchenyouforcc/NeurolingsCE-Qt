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

#include "shijima-qt/ShijimaLocalApi.hpp"
#include "shijima-qt/SecurityLimits.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>

namespace {

QString const kCommandKey = QStringLiteral("command");

QByteArray jsonMessage(QJsonObject const& object) {
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

bool parseObject(QByteArray const& bytes, QJsonObject &object, QString &error) {
    QJsonParseError parseError;
    auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Failed to parse IPC response: %1")
            .arg(parseError.errorString());
        return false;
    }
    object = document.object();
    return true;
}

bool readMessage(QLocalSocket &socket, int timeoutMs, QByteArray &message,
    QString &error)
{
    message.clear();
    while (true) {
        auto newlineIndex = message.indexOf('\n');
        if (newlineIndex >= 0) {
            message.truncate(newlineIndex);
            return true;
        }
        if (socket.state() != QLocalSocket::ConnectedState &&
            !socket.bytesAvailable())
        {
            error = QStringLiteral("IPC connection closed before a complete response was received");
            return false;
        }
        if (!socket.bytesAvailable() && !socket.waitForReadyRead(timeoutMs)) {
            error = QStringLiteral("Timed out waiting for IPC response");
            return false;
        }
        message.append(socket.readAll());
        if (message.size() > static_cast<qsizetype>(SecurityLimits::kIpcMessageMaxBytes)) {
            error = QStringLiteral("IPC message exceeds the maximum size of %1 bytes")
                .arg(SecurityLimits::kIpcMessageMaxBytes);
            return false;
        }
    }
}

bool writeMessage(QLocalSocket &socket, QJsonObject const& object,
    int timeoutMs, QString &error)
{
    auto bytes = jsonMessage(object);
    if (bytes.size() > static_cast<qsizetype>(SecurityLimits::kIpcMessageMaxBytes)) {
        error = QStringLiteral("IPC request exceeds the maximum size of %1 bytes")
            .arg(SecurityLimits::kIpcMessageMaxBytes);
        return false;
    }
    if (socket.write(bytes) != bytes.size()) {
        error = QStringLiteral("Failed to write IPC request");
        return false;
    }
    if (!socket.waitForBytesWritten(timeoutMs)) {
        error = QStringLiteral("Timed out sending IPC request");
        return false;
    }
    return true;
}

bool pingServer(QString const& serverName, int connectTimeoutMs,
    int readTimeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(connectTimeoutMs)) {
        return false;
    }
    QString error;
    if (!writeMessage(socket, QJsonObject {
        { kCommandKey, QStringLiteral("ping") },
    }, connectTimeoutMs, error))
    {
        return false;
    }
    QByteArray bytes;
    if (!readMessage(socket, readTimeoutMs, bytes, error)) {
        return false;
    }
    QJsonObject response;
    if (!parseObject(bytes, response, error)) {
        return false;
    }
    return response.value("ok").toBool(false);
}

}

QString shijimaLocalApiServerName() {
    return QStringLiteral("io.github.qingchenyouforcc.NeurolingsCE.cli");
}

bool shijimaLocalApiRequest(QJsonObject const& request, QJsonObject &response,
    QString &transportError, ShijimaLocalApiClientOptions const& options)
{
    QLocalSocket socket;
    socket.connectToServer(shijimaLocalApiServerName());
    if (!socket.waitForConnected(options.connectTimeoutMs)) {
        transportError = QStringLiteral("NeurolingsCE runtime is not available");
        return false;
    }
    if (!writeMessage(socket, request, options.connectTimeoutMs, transportError)) {
        return false;
    }
    QByteArray bytes;
    if (!readMessage(socket, options.readTimeoutMs, bytes, transportError)) {
        return false;
    }
    return parseObject(bytes, response, transportError);
}

bool shijimaLocalApiPing(ShijimaLocalApiClientOptions const& options) {
    return pingServer(shijimaLocalApiServerName(), options.connectTimeoutMs,
        options.readTimeoutMs);
}

bool shijimaLocalApiShowManager(ShijimaLocalApiClientOptions const& options) {
    QJsonObject response;
    QString transportError;
    if (!shijimaLocalApiRequest(QJsonObject {
        { kCommandKey, QStringLiteral("show_manager") },
    }, response, transportError, options))
    {
        return false;
    }

    return !response.contains(QStringLiteral("error"));
}
