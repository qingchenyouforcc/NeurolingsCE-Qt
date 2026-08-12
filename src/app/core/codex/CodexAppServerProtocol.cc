#include "shijima-qt/CodexAppServerProtocol.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtMath>
#include <QtGlobal>

namespace {

constexpr int kMaxPlanSteps = 32;
constexpr int kMaxPlanStepText = 2048;
constexpr int kMaxQuestionCount = 3;

QString bounded(QString value, int max) {
    if (value.size() <= max) {
        return value;
    }
    return value.left(max);
}

bool validId(QJsonValue const& id) {
    if (id.isString()) {
        return true;
    }
    if (id.isDouble()) {
        double number = id.toDouble();
        return qIsFinite(number) && number >= -9007199254740991.0 && number <= 9007199254740991.0
            && qFloor(number) == number;
    }
    return false;
}

QString valueString(QJsonObject const& object, char const* key) {
    QJsonValue value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : QString();
}

QJsonObject nestedObject(QJsonObject const& object, char const* key) {
    QJsonValue value = object.value(QLatin1String(key));
    return value.isObject() ? value.toObject() : QJsonObject();
}

QJsonArray nestedArray(QJsonObject const& object, char const* key) {
    QJsonValue value = object.value(QLatin1String(key));
    return value.isArray() ? value.toArray() : QJsonArray();
}

QJsonObject boundedNetworkContext(QJsonObject const& object) {
    QJsonObject result;
    for (QString const& key : { QStringLiteral("host"), QStringLiteral("protocol") }) {
        QJsonValue value = object.value(key);
        if (value.isString()) result.insert(key, bounded(value.toString(), 512));
    }
    QJsonValue port = object.value(QStringLiteral("port"));
    if (port.isString()) result.insert(QStringLiteral("port"), bounded(port.toString(), 32));
    else if (port.isDouble() && qIsFinite(port.toDouble())) result.insert(QStringLiteral("port"), port);
    return result;
}

void setError(QString* error, QString const& message) {
    if (error != nullptr) {
        *error = message;
    }
}

QByteArray compact(QJsonObject object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject itemObject(QJsonObject const& params) {
    if (params.value(QStringLiteral("item")).isObject()) {
        return params.value(QStringLiteral("item")).toObject();
    }
    return params;
}

}

QString codexServerStateName(CodexServerState state) {
    switch (state) {
    case CodexServerState::Stopped: return QStringLiteral("stopped");
    case CodexServerState::Starting: return QStringLiteral("starting");
    case CodexServerState::Initializing: return QStringLiteral("initializing");
    case CodexServerState::Ready: return QStringLiteral("ready");
    case CodexServerState::Running: return QStringLiteral("running");
    case CodexServerState::NeedsInput: return QStringLiteral("needsInput");
    case CodexServerState::Blocked: return QStringLiteral("blocked");
    case CodexServerState::Stopping: return QStringLiteral("stopping");
    }
    return QStringLiteral("blocked");
}

QString codexApprovalKindName(CodexApprovalKind kind) {
    switch (kind) {
    case CodexApprovalKind::CommandExecution: return QStringLiteral("command");
    case CodexApprovalKind::FileChange: return QStringLiteral("file");
    case CodexApprovalKind::Network: return QStringLiteral("network");
    }
    return QStringLiteral("command");
}

QString codexApprovalDecisionName(CodexApprovalDecision decision) {
    switch (decision) {
    case CodexApprovalDecision::Accept: return QStringLiteral("accept");
    case CodexApprovalDecision::AcceptForSession: return QStringLiteral("acceptForSession");
    case CodexApprovalDecision::Decline: return QStringLiteral("decline");
    case CodexApprovalDecision::Cancel: return QStringLiteral("cancel");
    }
    return QStringLiteral("cancel");
}

bool codexApprovalDecisionFromName(QString const& name,
    CodexApprovalDecision& decision) {
    if (name == QStringLiteral("accept")) {
        decision = CodexApprovalDecision::Accept;
    }
    else if (name == QStringLiteral("acceptForSession")) {
        decision = CodexApprovalDecision::AcceptForSession;
    }
    else if (name == QStringLiteral("decline")) {
        decision = CodexApprovalDecision::Decline;
    }
    else if (name == QStringLiteral("cancel")) {
        decision = CodexApprovalDecision::Cancel;
    }
    else {
        return false;
    }
    return true;
}

QString codexApprovalRequestIdKey(QJsonValue const& id) {
    if (id.isString()) {
        return QStringLiteral("s:") + id.toString();
    }
    if (id.isDouble()) {
        return QStringLiteral("n:") + QString::number(id.toDouble(), 'g', 17);
    }
    return QString();
}

bool parseCodexJsonRpcMessage(QByteArray const& bytes,
    CodexJsonRpcMessage& message, QString* errorMessage) {
    message = {};
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("JSON-RPC message must be an object"));
        return false;
    }
    QJsonObject object = document.object();
    if (object.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0")) {
        setError(errorMessage, QStringLiteral("missing JSON-RPC version"));
        return false;
    }

    bool hasId = object.contains(QStringLiteral("id"));
    bool hasMethod = object.value(QStringLiteral("method")).isString();
    bool hasParams = object.contains(QStringLiteral("params"));
    bool hasResult = object.contains(QStringLiteral("result"));
    bool hasError = object.contains(QStringLiteral("error"));
    if (hasMethod) {
        if (object.value(QStringLiteral("method")).toString().isEmpty()) {
            setError(errorMessage, QStringLiteral("method must not be empty"));
            return false;
        }
        if (hasResult || hasError) {
            setError(errorMessage, QStringLiteral("method message cannot contain result/error"));
            return false;
        }
        if (!hasParams || !object.value(QStringLiteral("params")).isObject()) {
            setError(errorMessage, QStringLiteral("params must be an object"));
            return false;
        }
        message.method = object.value(QStringLiteral("method")).toString();
        message.params = hasParams ? object.value(QStringLiteral("params")).toObject()
                                   : QJsonObject();
        if (hasId) {
            message.id = object.value(QStringLiteral("id"));
            if (!validId(message.id)) {
                setError(errorMessage, QStringLiteral("request id must be a string or safe integer"));
                return false;
            }
            message.kind = CodexJsonRpcMessageKind::Request;
        }
        else {
            message.kind = CodexJsonRpcMessageKind::Notification;
        }
        return true;
    }
    if (!hasId || (!hasResult && !hasError) || (hasResult && hasError)) {
        setError(errorMessage, QStringLiteral("invalid JSON-RPC response envelope"));
        return false;
    }
    message.id = object.value(QStringLiteral("id"));
    if (!validId(message.id)) {
        setError(errorMessage, QStringLiteral("response id must be a string or safe integer"));
        return false;
    }
    if (hasError) {
        if (!object.value(QStringLiteral("error")).isObject()) {
            setError(errorMessage, QStringLiteral("error must be an object"));
            return false;
        }
        QJsonObject error = object.value(QStringLiteral("error")).toObject();
        if (!error.value(QStringLiteral("code")).isDouble() ||
            !error.value(QStringLiteral("message")).isString()) {
            setError(errorMessage, QStringLiteral("error requires code and message"));
            return false;
        }
    }
    message.kind = CodexJsonRpcMessageKind::Response;
    message.result = object.value(QStringLiteral("result"));
    message.error = object.value(QStringLiteral("error")).toObject();
    return true;
}

QByteArray codexJsonRpcRequest(QJsonValue const& id, QString const& method,
    QJsonObject const& params) {
    QJsonObject object {
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params }
    };
    return compact(object);
}

QByteArray codexJsonRpcNotification(QString const& method,
    QJsonObject const& params) {
    return compact({
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params }
    });
}

QByteArray codexJsonRpcResult(QJsonValue const& id, QJsonValue const& result) {
    return compact({
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("result"), result }
    });
}

QByteArray codexJsonRpcError(QJsonValue const& id, int code,
    QString const& message, QJsonValue const& data) {
    QJsonObject error {
        { QStringLiteral("code"), code },
        { QStringLiteral("message"), message }
    };
    if (!data.isUndefined()) {
        error.insert(QStringLiteral("data"), data);
    }
    return compact({
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("error"), error }
    });
}

QByteArray codexUnsupportedRequest(QJsonValue const& id, QString const&) {
    return codexJsonRpcError(id, -32601, QStringLiteral("Method not supported"));
}

bool parseCodexApprovalRequest(QString const& method, QJsonValue const& id,
    QJsonObject const& params, CodexApprovalRequest& request,
    QString* errorMessage) {
    bool command = method == QStringLiteral("item/commandExecution/requestApproval");
    bool file = method == QStringLiteral("item/fileChange/requestApproval");
    if (!command && !file) {
        setError(errorMessage, QStringLiteral("unsupported approval method"));
        return false;
    }
    request = {};
    request.requestId = id;
    request.kind = file ? CodexApprovalKind::FileChange : CodexApprovalKind::CommandExecution;
    QJsonObject item = itemObject(params);
    request.threadId = valueString(params, "threadId");
    request.turnId = valueString(params, "turnId");
    request.itemId = valueString(params, "itemId");
    if (request.threadId.isEmpty()) request.threadId = valueString(item, "threadId");
    if (request.turnId.isEmpty()) request.turnId = valueString(item, "turnId");
    if (request.itemId.isEmpty()) request.itemId = valueString(item, "id");
    request.reason = bounded(valueString(params, "reason"), 4096);
    if (request.reason.isEmpty()) request.reason = bounded(valueString(item, "reason"), 4096);
    request.command = bounded(valueString(params, "command"), 131072);
    if (request.command.isEmpty()) request.command = bounded(valueString(item, "command"), 131072);
    request.cwd = bounded(valueString(params, "cwd"), 4096);
    if (request.cwd.isEmpty()) request.cwd = bounded(valueString(item, "cwd"), 4096);
    QJsonArray actions = nestedArray(params, "commandActions");
    if (actions.isEmpty()) actions = nestedArray(item, "commandActions");
    for (QJsonValue const& value : actions) {
        if (request.commandActions.size() >= 32) break;
        if (!value.isObject()) continue;
        QJsonObject action = value.toObject();
        request.commandActions.push_back({
            bounded(valueString(action, "type"), 128),
            bounded(valueString(action, "command"), 131072),
            bounded(valueString(action, "description"), 4096)
        });
    }
    QJsonArray changes = nestedArray(params, "changes");
    if (changes.isEmpty()) changes = nestedArray(item, "changes");
    for (QJsonValue const& value : changes) {
        if (request.changes.size() >= 256) break;
        if (!value.isObject()) continue;
        QJsonObject change = value.toObject();
        request.changes.push_back({
            bounded(valueString(change, "path"), 4096),
            valueString(change, "kind").isEmpty()
                ? valueString(change, "type") : valueString(change, "kind"),
            bounded(valueString(change, "diff"), 131072)
        });
    }
    request.networkContext = nestedObject(params, "networkApprovalContext");
    if (request.networkContext.isEmpty()) {
        request.networkContext = nestedObject(item, "networkApprovalContext");
    }
    request.networkContext = boundedNetworkContext(request.networkContext);
    if (!request.networkContext.isEmpty()) {
        request.kind = CodexApprovalKind::Network;
    }
    QJsonArray decisions = nestedArray(params, "availableDecisions");
    for (QJsonValue const& value : decisions) {
        if (value.isString()) request.availableDecisions.push_back(value.toString());
    }
    if (request.availableDecisions.isEmpty()) {
        request.availableDecisions = {
            QStringLiteral("accept"), QStringLiteral("acceptForSession"),
            QStringLiteral("decline"), QStringLiteral("cancel")
        };
    }
    return true;
}

bool parseCodexPlanSnapshot(QString const& method, QJsonObject const& params,
    CodexPlanSnapshot& snapshot, QString* errorMessage) {
    if (method != QStringLiteral("turn/plan/updated") &&
        method != QStringLiteral("item/plan/delta") &&
        method != QStringLiteral("item/completed")) {
        setError(errorMessage, QStringLiteral("unsupported plan method"));
        return false;
    }
    snapshot = {};
    snapshot.threadId = valueString(params, "threadId");
    snapshot.turnId = valueString(params, "turnId");
    snapshot.itemId = valueString(params, "itemId");
    QJsonObject item = itemObject(params);
    if (snapshot.threadId.isEmpty()) snapshot.threadId = valueString(item, "threadId");
    if (snapshot.turnId.isEmpty()) snapshot.turnId = valueString(item, "turnId");
    if (snapshot.itemId.isEmpty()) snapshot.itemId = valueString(item, "id");
    snapshot.explanation = bounded(valueString(params, "explanation"), kMaxPlanStepText);
    if (snapshot.explanation.isEmpty()) snapshot.explanation = bounded(valueString(item, "explanation"), kMaxPlanStepText);
    snapshot.finalText = bounded(valueString(params, "text"), 4096);
    if (snapshot.finalText.isEmpty()) snapshot.finalText = bounded(valueString(item, "text"), 4096);
    snapshot.final = method == QStringLiteral("item/completed");
    QJsonArray steps = nestedArray(params, "plan");
    if (steps.isEmpty()) steps = nestedArray(params, "steps");
    if (steps.isEmpty()) steps = nestedArray(item, "steps");
    for (QJsonValue const& value : steps) {
        if (snapshot.steps.size() >= kMaxPlanSteps) break;
        CodexPlanStep step;
        if (value.isString()) {
            step.step = bounded(value.toString(), kMaxPlanStepText);
            step.status = QStringLiteral("pending");
        }
        else if (value.isObject()) {
            auto object = value.toObject();
            step.step = bounded(valueString(object, "step"), kMaxPlanStepText);
            if (step.step.isEmpty()) step.step = bounded(valueString(object, "text"), kMaxPlanStepText);
            step.status = valueString(object, "status");
            if (step.status != QStringLiteral("pending") &&
                step.status != QStringLiteral("inProgress") &&
                step.status != QStringLiteral("completed")) {
                step.status = QStringLiteral("pending");
            }
        }
        if (!step.step.isEmpty()) snapshot.steps.push_back(step);
    }
    return true;
}

bool parseCodexUserInputRequest(QString const& method, QJsonValue const& id,
    QJsonObject const& params, CodexUserInputRequest& request,
    QString* errorMessage) {
    if (method != QStringLiteral("item/tool/requestUserInput")) {
        setError(errorMessage, QStringLiteral("unsupported user input method"));
        return false;
    }
    request = {};
    request.requestId = id;
    request.threadId = valueString(params, "threadId");
    request.turnId = valueString(params, "turnId");
    request.itemId = valueString(params, "itemId");
    request.autoResolutionMs = qBound(0,
        params.value(QStringLiteral("autoResolutionMs")).toInt(0), 3600000);
    QJsonArray questions = nestedArray(params, "questions");
    if (questions.isEmpty()) {
        setError(errorMessage, QStringLiteral("at least one question is required"));
        return false;
    }
    if (questions.size() > kMaxQuestionCount) {
        setError(errorMessage, QStringLiteral("at most three questions are supported"));
        return false;
    }
    for (QJsonValue const& value : questions) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("question must be an object"));
            return false;
        }
        QJsonObject object = value.toObject();
        CodexUserInputQuestion question;
        question.id = bounded(valueString(object, "id"), 256);
        question.header = bounded(valueString(object, "header"), 512);
        question.question = bounded(valueString(object, "question"), 4096);
        question.isOther = object.value(QStringLiteral("isOther")).toBool(false);
        question.isSecret = object.value(QStringLiteral("isSecret")).toBool(false);
        for (QJsonValue const& optionValue : nestedArray(object, "options")) {
            if (!optionValue.isObject()) continue;
            QJsonObject option = optionValue.toObject();
            question.options.push_back({
                bounded(valueString(option, "label"), 1024),
                bounded(valueString(option, "description"), 2048)
            });
            if (question.options.size() >= 3) break;
        }
        if (question.id.isEmpty() || question.question.isEmpty()) {
            setError(errorMessage, QStringLiteral("question id and question are required"));
            return false;
        }
        request.questions.push_back(question);
    }
    return true;
}
