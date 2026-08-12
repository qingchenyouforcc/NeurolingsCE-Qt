#pragma once

#include "shijima-qt/CodexAppServerModels.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <QString>

enum class CodexJsonRpcMessageKind {
    Invalid,
    Request,
    Response,
    Notification
};

struct CodexJsonRpcMessage {
    CodexJsonRpcMessageKind kind = CodexJsonRpcMessageKind::Invalid;
    QJsonValue id;
    QString method;
    QJsonObject params;
    QJsonValue result;
    QJsonObject error;
    QString errorMessage;
};

bool parseCodexJsonRpcMessage(QByteArray const& bytes,
    CodexJsonRpcMessage& message, QString* errorMessage = nullptr);
QByteArray codexJsonRpcRequest(QJsonValue const& id, QString const& method,
    QJsonObject const& params = {});
QByteArray codexJsonRpcNotification(QString const& method,
    QJsonObject const& params = {});
QByteArray codexJsonRpcResult(QJsonValue const& id, QJsonValue const& result);
QByteArray codexJsonRpcError(QJsonValue const& id, int code,
    QString const& message, QJsonValue const& data = {});

bool parseCodexApprovalRequest(QString const& method, QJsonValue const& id,
    QJsonObject const& params, CodexApprovalRequest& request,
    QString* errorMessage = nullptr);
bool parseCodexPlanSnapshot(QString const& method, QJsonObject const& params,
    CodexPlanSnapshot& snapshot, QString* errorMessage = nullptr);
bool parseCodexUserInputRequest(QString const& method, QJsonValue const& id,
    QJsonObject const& params, CodexUserInputRequest& request,
    QString* errorMessage = nullptr);

// Returns a bounded, deterministic JSON-RPC error for an incoming server
// request. The method is deliberately not logged with user supplied params.
QByteArray codexUnsupportedRequest(QJsonValue const& id,
    QString const& method);

