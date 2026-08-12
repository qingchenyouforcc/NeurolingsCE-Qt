#include "shijima-qt/CodexAppServerClient.hpp"
#include "shijima-qt/CodexAppServerProtocol.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include <algorithm>

namespace {

constexpr qsizetype kMaxLineBytes = 4 * 1024 * 1024;
constexpr qsizetype kMaxBufferBytes = 8 * 1024 * 1024;
constexpr qsizetype kMaxStderrBytes = 64 * 1024;

QString idKey(QJsonValue const& id) {
    return codexApprovalRequestIdKey(id);
}

QString stringValue(QJsonObject const& object, QString const& key) {
    return object.value(key).isString() ? object.value(key).toString() : QString();
}

QJsonObject objectValue(QJsonObject const& object, QString const& key) {
    return object.value(key).isObject() ? object.value(key).toObject() : QJsonObject();
}

QJsonObject collaborationPreset(QJsonObject const& value, QString const& modeName) {
    QJsonObject settings = objectValue(value, QStringLiteral("settings"));
    // Older app-server builds returned the preset fields alongside `mode`;
    // normalize those fields into the v2 collaborationMode.settings object.
    for (QString const& key : { QStringLiteral("model"),
        QStringLiteral("reasoning_effort"), QStringLiteral("developer_instructions") }) {
        if (!settings.contains(key) && value.contains(key)) settings.insert(key, value.value(key));
    }
    if (!settings.contains(QStringLiteral("developer_instructions"))) {
        settings.insert(QStringLiteral("developer_instructions"), QJsonValue::Null);
    }
    return QJsonObject {
        { QStringLiteral("mode"), modeName },
        { QStringLiteral("settings"), settings }
    };
}

bool arrayAllows(QJsonValue const& value, QStringList const& allowed) {
    if (!value.isArray()) return true;
    QJsonArray values = value.toArray();
    if (values.isEmpty()) return false;
    for (QJsonValue const& candidate : values) {
        if (!candidate.isString()) continue;
        if (allowed.contains(candidate.toString())) return true;
    }
    return false;
}

bool configRequirementsAllow(QJsonValue const& value) {
    // `null` is the documented response when no managed requirements exist.
    if (value.isNull() || value.isUndefined()) return true;
    if (!value.isObject()) return false;
    QJsonObject object = value.toObject();
    if (!arrayAllows(object.value(QStringLiteral("allowedApprovalPolicies")),
        { QStringLiteral("on-request") })) return false;
    if (!arrayAllows(object.value(QStringLiteral("allowedSandboxModes")),
        { QStringLiteral("workspace-write"), QStringLiteral("workspaceWrite") })) return false;
    auto policy = object.value(QStringLiteral("approvalPolicy"));
    auto sandbox = object.value(QStringLiteral("sandbox"));
    QJsonObject policyObject = policy.isObject() ? policy.toObject() : QJsonObject();
    QJsonObject sandboxObject = sandbox.isObject() ? sandbox.toObject() : QJsonObject();
    if (policy.isString() && policy.toString() != QStringLiteral("on-request")) return false;
    if (sandbox.isString() && sandbox.toString() != QStringLiteral("workspace-write") &&
        sandbox.toString() != QStringLiteral("workspaceWrite")) return false;
    if (policyObject.value(QStringLiteral("allowed")).isBool() &&
        !policyObject.value(QStringLiteral("allowed")).toBool()) return false;
    if (sandboxObject.value(QStringLiteral("allowed")).isBool() &&
        !sandboxObject.value(QStringLiteral("allowed")).toBool()) return false;
    return true;
}

}

bool CodexApprovalStore::insert(CodexApprovalRequest request) {
    QString key = idKey(request.requestId);
    if (key.isEmpty() || m_requests.contains(key) || m_requests.size() >= kMaxPending) {
        return false;
    }
    request.sequence = m_nextSequence++;
    m_requests.insert(key, std::move(request));
    return true;
}

bool CodexApprovalStore::contains(QJsonValue const& id) const {
    return m_requests.contains(idKey(id));
}

bool CodexApprovalStore::updateChanges(QJsonValue const& id,
    QList<CodexFileChange> changes) {
    auto it = m_requests.find(idKey(id));
    if (it == m_requests.end()) return false;
    it->changes = std::move(changes);
    return true;
}

bool CodexApprovalStore::take(QJsonValue const& id, CodexApprovalRequest* request) {
    auto it = m_requests.find(idKey(id));
    if (it == m_requests.end()) return false;
    if (request != nullptr) *request = it.value();
    m_requests.erase(it);
    return true;
}

CodexApprovalRequest const* CodexApprovalStore::find(QJsonValue const& id) const {
    auto it = m_requests.constFind(idKey(id));
    return it == m_requests.constEnd() ? nullptr : &it.value();
}

QList<CodexApprovalRequest> CodexApprovalStore::values() const {
    QList<CodexApprovalRequest> result = m_requests.values();
    std::sort(result.begin(), result.end(), [](CodexApprovalRequest const& left,
        CodexApprovalRequest const& right) { return left.sequence < right.sequence; });
    return result;
}

void CodexApprovalStore::clear() {
    m_requests.clear();
}

CodexAppServerClient::CodexAppServerClient(QObject* parent): QObject(parent) {
    qRegisterMetaType<CodexServerState>();
    qRegisterMetaType<CodexApprovalRequest>();
    qRegisterMetaType<CodexUserInputRequest>();
    qRegisterMetaType<CodexPlanSnapshot>();
}

CodexAppServerClient::~CodexAppServerClient() {
    stop();
}

void CodexAppServerClient::setExecutable(QString path) {
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) return;
    QString trimmed = path.trimmed();
    m_executable = trimmed.isEmpty() ? QString() : QDir::cleanPath(trimmed);
}

void CodexAppServerClient::setState(CodexServerState state) {
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(m_state);
}

bool CodexAppServerClient::start() {
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        // A blocked connection is never reused implicitly.  The explicit
        // reconnect action may tear it down and start a fresh generation.
        if (m_state != CodexServerState::Blocked) return true;
        stop();
    }
    if (m_process != nullptr && m_process->state() == QProcess::NotRunning) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    QString executable = m_executable;
    if (executable.isEmpty()) executable = QStandardPaths::findExecutable(QStringLiteral("codex"));
    if (executable.isEmpty() || executable.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive) ||
        executable.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive)) {
        setState(CodexServerState::Blocked);
        emit diagnostic(QStringLiteral("Codex executable not found; choose an actual executable"));
        return false;
    }
    m_executable = executable;
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_initialized = false;
    m_configRequirementsAllowed = false;
    m_stopping = false;
    ++m_connectionGeneration;
    m_callbacks.clear();
    m_approvals.clear();
    m_userInputs.clear();
    m_planDeltas.clear();
    m_fileChangesByItem.clear();
    m_availableModes.clear();
    m_defaultPreset = {};
    m_planPreset = {};
    m_planSupported = false;
    m_threadId.clear();
    m_turnId.clear();
    m_workspace.clear();
    m_plan = {};
    m_finalMessage.clear();
    m_process = new QProcess(this);
    m_process->setProgram(m_executable);
    m_process->setArguments({ QStringLiteral("app-server"), QStringLiteral("--listen"), QStringLiteral("stdio://") });
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CodexAppServerClient::readStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &CodexAppServerClient::readStderr);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &CodexAppServerClient::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CodexAppServerClient::processError);
    setState(CodexServerState::Starting);
    m_process->start();
    if (!m_process->waitForStarted(2000)) {
        failClosed(QStringLiteral("Codex app-server failed to start"));
        return false;
    }
    setState(CodexServerState::Initializing);
    QJsonObject params {
        { QStringLiteral("clientInfo"), QJsonObject {
            { QStringLiteral("name"), QStringLiteral("neurolingsce") },
            { QStringLiteral("version"), QStringLiteral("1") }
        } },
        { QStringLiteral("capabilities"), QJsonObject {{ QStringLiteral("experimentalApi"), true }} },
        { QStringLiteral("optOutNotificationMethods"), QJsonArray {
            QStringLiteral("item/commandExecution/outputDelta"),
            QStringLiteral("item/reasoning/summaryTextDelta")
        } }
    };
    sendRequest(QStringLiteral("initialize"), params,
        [this](QJsonValue const&, QJsonValue const& result, QJsonObject const& error) {
            if (!error.isEmpty()) { failClosed(QStringLiteral("initialize failed")); return; }
            Q_UNUSED(result);
            m_initialized = true;
            sendNotification(QStringLiteral("initialized"), {});
            sendRequest(QStringLiteral("configRequirements/read"), {},
                [this](QJsonValue const&, QJsonValue const& value, QJsonObject const& errorObject) {
                    if (!errorObject.isEmpty()) {
                        emit diagnostic(QStringLiteral("Codex configuration requirements could not be read"));
                        return;
                    }
                    m_configRequirementsAllowed = configRequirementsAllow(value);
                    if (!m_configRequirementsAllowed) {
                        emit diagnostic(QStringLiteral("Codex administrator restrictions are active"));
                    }
                });
            sendRequest(QStringLiteral("collaborationMode/list"), {},
                [this](QJsonValue const&, QJsonValue const& value, QJsonObject const& errorObject) {
                    if (!errorObject.isEmpty() || !value.isObject()) {
                        m_planSupported = false;
                        setState(CodexServerState::Ready);
                        return;
                    }
                    QJsonObject result = value.toObject();
                    QJsonArray modes = result.value(QStringLiteral("data")).toArray();
                    if (modes.isEmpty()) modes = result.value(QStringLiteral("modes")).toArray();
                    for (QJsonValue const& modeValue : modes) {
                        QJsonObject mode;
                        QString name;
                        if (modeValue.isString()) {
                            name = modeValue.toString();
                            mode = QJsonObject {{ QStringLiteral("mode"), name }};
                        }
                        else if (modeValue.isObject()) {
                            mode = modeValue.toObject();
                            name = stringValue(mode, QStringLiteral("mode"));
                            if (name.isEmpty()) name = stringValue(mode, QStringLiteral("name"));
                        }
                        if (name.isEmpty()) continue;
                        if (!name.isEmpty()) m_availableModes.push_back(name);
                        if (name.compare(QStringLiteral("plan"), Qt::CaseInsensitive) == 0) {
                            m_planSupported = true;
                            m_planPreset = collaborationPreset(mode, QStringLiteral("plan"));
                        }
                        else if (name.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0) {
                            m_defaultPreset = collaborationPreset(mode, QStringLiteral("default"));
                        }
                    }
                    setState(CodexServerState::Ready);
                });
        });
    return true;
}

void CodexAppServerClient::stop() {
    if (m_process == nullptr) {
        setState(CodexServerState::Stopped);
        return;
    }
    m_stopping = true;
    setState(CodexServerState::Stopping);
    cancelPendingRequests();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(500)) m_process->kill();
    }
    m_process->deleteLater();
    m_process = nullptr;
    m_callbacks.clear();
    m_userInputs.clear();
    m_approvals.clear();
    m_threadId.clear();
    m_turnId.clear();
    m_stopping = false;
    setState(CodexServerState::Stopped);
}

void CodexAppServerClient::sendRequest(QString const& method, QJsonObject const& params,
    std::function<void(QJsonValue const&, QJsonValue const&, QJsonObject const&)> callback) {
    if (m_process == nullptr || m_process->state() != QProcess::Running) return;
    QJsonValue id(static_cast<double>(m_nextRequestId++));
    if (callback) m_callbacks.insert(idKey(id), std::move(callback));
    writeJson(codexJsonRpcRequest(id, method, params));
}

void CodexAppServerClient::sendNotification(QString const& method, QJsonObject const& params) {
    if (m_process != nullptr && m_process->state() == QProcess::Running) writeJson(codexJsonRpcNotification(method, params));
}

void CodexAppServerClient::writeJson(QByteArray const& payload) {
    if (m_process == nullptr || m_process->state() != QProcess::Running) return;
    m_process->write(payload);
}

void CodexAppServerClient::startNewThread(QString const& cwd) {
    if (!m_initialized || !m_configRequirementsAllowed ||
        m_state != CodexServerState::Ready) return;
    if (!m_threadId.isEmpty()) {
        sendRequest(QStringLiteral("thread/unsubscribe"), {{ QStringLiteral("threadId"), m_threadId }});
        m_threadId.clear();
        m_turnId.clear();
        cancelPendingRequests();
        m_callbacks.clear();
    }
    QJsonObject params {
        { QStringLiteral("approvalPolicy"), QStringLiteral("on-request") },
        { QStringLiteral("sandbox"), QStringLiteral("workspace-write") },
        { QStringLiteral("serviceName"), QStringLiteral("neurolingsce") }
    };
    if (!cwd.trimmed().isEmpty()) params.insert(QStringLiteral("cwd"), cwd);
    sendRequest(QStringLiteral("thread/start"), params,
        [this, cwd](QJsonValue const&, QJsonValue const& value, QJsonObject const& error) {
            if (!error.isEmpty() || !value.isObject()) { failClosed(QStringLiteral("thread/start failed")); return; }
            QJsonObject object = value.toObject();
            QJsonObject thread = objectValue(object, QStringLiteral("thread"));
            m_threadId = stringValue(thread, QStringLiteral("id"));
            if (m_threadId.isEmpty()) m_threadId = stringValue(object, QStringLiteral("threadId"));
            if (m_threadId.isEmpty()) m_threadId = stringValue(object, QStringLiteral("id"));
            m_workspace = stringValue(thread, QStringLiteral("cwd"));
            if (m_workspace.isEmpty()) m_workspace = cwd;
            emit threadChanged(m_threadId, m_workspace);
            setState(CodexServerState::Ready);
        });
}

void CodexAppServerClient::resumeThread(QString const& threadId) {
    if (!m_initialized || !m_configRequirementsAllowed || m_state != CodexServerState::Ready ||
        threadId.trimmed().isEmpty()) return;
    if (!m_threadId.isEmpty() && m_threadId != threadId) {
        sendRequest(QStringLiteral("thread/unsubscribe"), {{ QStringLiteral("threadId"), m_threadId }});
        m_threadId.clear();
        m_turnId.clear();
        cancelPendingRequests();
        m_callbacks.clear();
    }
    sendRequest(QStringLiteral("thread/resume"), {{ QStringLiteral("threadId"), threadId }},
        [this](QJsonValue const&, QJsonValue const& value, QJsonObject const& error) {
            if (!error.isEmpty() || !value.isObject()) { failClosed(QStringLiteral("thread/resume failed")); return; }
            auto object = value.toObject();
            QJsonObject thread = objectValue(object, QStringLiteral("thread"));
            m_threadId = stringValue(thread, QStringLiteral("id"));
            if (m_threadId.isEmpty()) m_threadId = stringValue(object, QStringLiteral("threadId"));
            if (m_threadId.isEmpty()) m_threadId = stringValue(object, QStringLiteral("id"));
            QString workspace = stringValue(thread, QStringLiteral("cwd"));
            if (!workspace.isEmpty()) m_workspace = workspace;
            emit threadChanged(m_threadId, m_workspace);
            setState(CodexServerState::Ready);
        });
}

void CodexAppServerClient::startTurn(QString const& text, bool planMode) {
    if (m_threadId.isEmpty() || text.trimmed().isEmpty() || m_state == CodexServerState::Running) return;
    if (planMode && (!m_planSupported || m_planPreset.isEmpty())) {
        emit diagnostic(QStringLiteral("Plan mode is not supported by this Codex app-server"));
        return;
    }
    QJsonObject params {
        { QStringLiteral("threadId"), m_threadId },
        { QStringLiteral("input"), QJsonArray {
            QJsonObject {{ QStringLiteral("type"), QStringLiteral("text") }, { QStringLiteral("text"), text }}
        } }
    };
    if (planMode && m_planSupported && !m_planPreset.isEmpty()) {
        params.insert(QStringLiteral("collaborationMode"), m_planPreset);
    }
    else if (!planMode && !m_defaultPreset.isEmpty()) {
        params.insert(QStringLiteral("collaborationMode"), m_defaultPreset);
    }
    m_finalMessage.clear();
    m_plan = {};
    m_planDeltas.clear();
    m_plan.threadId = m_threadId;
    setState(CodexServerState::Running);
    sendRequest(QStringLiteral("turn/start"), params,
        [this](QJsonValue const&, QJsonValue const& value, QJsonObject const& error) {
            if (!error.isEmpty() || !value.isObject()) { failClosed(QStringLiteral("turn/start failed")); return; }
            auto object = value.toObject();
            QJsonObject turn = objectValue(object, QStringLiteral("turn"));
            m_turnId = stringValue(turn, QStringLiteral("id"));
            if (m_turnId.isEmpty()) m_turnId = stringValue(object, QStringLiteral("turnId"));
            if (m_turnId.isEmpty()) m_turnId = stringValue(object, QStringLiteral("id"));
        });
}

void CodexAppServerClient::steerTurn(QString const& text) {
    if (m_threadId.isEmpty() || m_turnId.isEmpty() || text.trimmed().isEmpty() || m_state != CodexServerState::Running) return;
    sendRequest(QStringLiteral("turn/steer"), {
        { QStringLiteral("threadId"), m_threadId },
        { QStringLiteral("expectedTurnId"), m_turnId },
        { QStringLiteral("input"), QJsonArray { QJsonObject {{ QStringLiteral("type"), QStringLiteral("text") }, { QStringLiteral("text"), text }} } }
    });
}

void CodexAppServerClient::interruptTurn() {
    if (m_threadId.isEmpty() || m_turnId.isEmpty() || m_state != CodexServerState::Running) return;
    sendRequest(QStringLiteral("turn/interrupt"), {{ QStringLiteral("threadId"), m_threadId }, { QStringLiteral("turnId"), m_turnId }});
}

bool CodexAppServerClient::resolveApproval(QJsonValue const& requestId,
    CodexApprovalDecision decision) {
    CodexApprovalRequest const* request = m_approvals.find(requestId);
    if (request == nullptr || request->connectionGeneration != m_connectionGeneration ||
        m_process == nullptr || m_process->state() != QProcess::Running) return false;
    QString decisionName = codexApprovalDecisionName(decision);
    if (!request->availableDecisions.contains(decisionName)) return false;
    writeJson(codexJsonRpcResult(requestId,
        QJsonObject {{ QStringLiteral("decision"), decisionName }}));
    m_approvals.take(requestId);
    emit approvalResolved(requestId);
    return true;
}

bool CodexAppServerClient::resolveUserInput(QJsonValue const& requestId,
    QJsonObject const& answers) {
    auto it = m_userInputs.find(idKey(requestId));
    if (it == m_userInputs.end() || it->connectionGeneration != m_connectionGeneration || m_process == nullptr || m_process->state() != QProcess::Running) return false;
    writeJson(codexJsonRpcResult(requestId,
        QJsonObject {{ QStringLiteral("answers"), answers }}));
    m_userInputs.erase(it);
    emit userInputResolved(requestId);
    setState(m_userInputs.isEmpty() ? CodexServerState::Running : CodexServerState::NeedsInput);
    return true;
}

void CodexAppServerClient::cancelPendingRequests() {
    if (m_process != nullptr && m_process->state() == QProcess::Running) {
        for (auto const& request : m_approvals.values()) {
            writeJson(codexJsonRpcResult(request.requestId,
                QJsonObject {{ QStringLiteral("decision"), QStringLiteral("cancel") }}));
        }
        for (auto const& request : m_userInputs) {
            writeJson(codexJsonRpcResult(request.requestId,
                QJsonObject {{ QStringLiteral("answers"), QJsonObject() }}));
        }
    }
    m_approvals.clear();
    m_userInputs.clear();
}

QList<CodexApprovalRequest> CodexAppServerClient::pendingApprovals() const {
    return m_approvals.values();
}

void CodexAppServerClient::readStdout() {
    if (m_process == nullptr || sender() != m_process) return;
    m_stdoutBuffer += m_process->readAllStandardOutput();
    if (m_stdoutBuffer.size() > kMaxBufferBytes) { failClosed(QStringLiteral("app-server output buffer exceeded limit")); return; }
    while (true) {
        qsizetype newline = m_stdoutBuffer.indexOf('\n');
        if (newline < 0) {
            if (m_stdoutBuffer.size() > kMaxLineBytes) failClosed(QStringLiteral("app-server line exceeded limit"));
            return;
        }
        QByteArray line = m_stdoutBuffer.left(newline);
        m_stdoutBuffer.remove(0, newline + 1);
        if (line.endsWith('\r')) line.chop(1);
        if (line.size() > kMaxLineBytes) { failClosed(QStringLiteral("app-server line exceeded limit")); return; }
        if (!line.trimmed().isEmpty()) handleMessage(line);
        if (m_state == CodexServerState::Blocked) return;
    }
}

void CodexAppServerClient::readStderr() {
    if (m_process == nullptr || sender() != m_process) return;
    QByteArray bytes = m_process->readAllStandardError();
    if (m_stderrBuffer.size() < kMaxStderrBytes) m_stderrBuffer += bytes.left(kMaxStderrBytes - m_stderrBuffer.size());
}

void CodexAppServerClient::processFinished(int, QProcess::ExitStatus) {
    if (sender() != m_process) return;
    if (!m_stdoutBuffer.trimmed().isEmpty() && m_state != CodexServerState::Blocked) {
        QByteArray finalLine = m_stdoutBuffer.trimmed();
        m_stdoutBuffer.clear();
        if (finalLine.size() <= kMaxLineBytes) handleMessage(finalLine);
        else failClosed(QStringLiteral("app-server line exceeded limit"));
    }
    if (!m_stopping && m_state != CodexServerState::Stopped) {
        QString reason = QStringLiteral("Codex app-server exited unexpectedly");
        if (!m_stderrBuffer.trimmed().isEmpty()) reason += QStringLiteral(": ") + QString::fromUtf8(m_stderrBuffer).left(512);
        failClosed(reason);
    }
    if (m_state == CodexServerState::Stopping || m_stopping) setState(CodexServerState::Stopped);
}

void CodexAppServerClient::processError(QProcess::ProcessError) {
    if (sender() != m_process) return;
    if (m_stopping) return;
    failClosed(QStringLiteral("Codex app-server process error"));
}

void CodexAppServerClient::handleMessage(QByteArray const& line) {
    CodexJsonRpcMessage message;
    QString error;
    if (!parseCodexJsonRpcMessage(line, message, &error)) { failClosed(QStringLiteral("invalid app-server JSON-RPC message")); return; }
    if (message.kind == CodexJsonRpcMessageKind::Response) {
        QString key = idKey(message.id);
        auto it = m_callbacks.find(key);
        if (it != m_callbacks.end()) {
            auto callback = std::move(it.value());
            m_callbacks.erase(it);
            callback(message.id, message.result, message.error);
        }
        return;
    }
    if (message.kind == CodexJsonRpcMessageKind::Request) {
        handleRequest(message.id, message.method, message.params);
        return;
    }
    if (message.kind == CodexJsonRpcMessageKind::Notification) handleNotification(message.method, message.params);
}

void CodexAppServerClient::handleRequest(QJsonValue const& id, QString const& method,
    QJsonObject const& params) {
    if (method == QStringLiteral("item/commandExecution/requestApproval") || method == QStringLiteral("item/fileChange/requestApproval")) {
        if (m_state != CodexServerState::Running && m_state != CodexServerState::NeedsInput) {
            writeJson(codexJsonRpcResult(id,
                QJsonObject {{ QStringLiteral("decision"), QStringLiteral("cancel") }}));
            return;
        }
        CodexApprovalRequest request;
        QString error;
        if (!parseCodexApprovalRequest(method, id, params, request, &error)) { writeJson(codexJsonRpcError(id, -32602, QStringLiteral("Invalid params"))); return; }
        request.connectionGeneration = m_connectionGeneration;
        if (request.kind == CodexApprovalKind::FileChange && request.changes.isEmpty()) {
            QString key = request.threadId + QLatin1Char('\x1f') + request.turnId + QLatin1Char('\x1f') + request.itemId;
            request.changes = m_fileChangesByItem.value(key);
        }
        if (!m_approvals.insert(request)) {
            writeJson(codexJsonRpcResult(id,
                QJsonObject {{ QStringLiteral("decision"), QStringLiteral("cancel") }}));
            if (m_approvals.size() >= CodexApprovalStore::kMaxPending) failClosed(QStringLiteral("too many pending approvals"));
            return;
        }
        emit approvalRequested(request);
        return;
    }
    if (method == QStringLiteral("item/tool/requestUserInput")) {
        if (m_state != CodexServerState::Running && m_state != CodexServerState::NeedsInput) {
            writeJson(codexJsonRpcResult(id,
                QJsonObject {{ QStringLiteral("answers"), QJsonObject() }}));
            return;
        }
        CodexUserInputRequest request;
        QString error;
        if (!parseCodexUserInputRequest(method, id, params, request, &error)) { writeJson(codexJsonRpcError(id, -32602, QStringLiteral("Invalid params"))); return; }
        request.connectionGeneration = m_connectionGeneration;
        QString key = idKey(id);
        if (m_userInputs.contains(key) || m_userInputs.size() >= 3) {
            writeJson(codexJsonRpcResult(id,
                QJsonObject {{ QStringLiteral("answers"), QJsonObject() }}));
            return;
        }
        m_userInputs.insert(key, request);
        setState(CodexServerState::NeedsInput);
        emit userInputRequested(request);
        return;
    }
    writeJson(codexUnsupportedRequest(id, method));
    setState(CodexServerState::Blocked);
}

void CodexAppServerClient::handleNotification(QString const& method, QJsonObject const& params) {
    if (method == QStringLiteral("thread/started")) {
        QJsonObject thread = objectValue(params, QStringLiteral("thread"));
        QString id = stringValue(thread, QStringLiteral("id"));
        if (id.isEmpty()) id = stringValue(params, QStringLiteral("threadId"));
        if (!id.isEmpty()) {
            m_threadId = id;
            QString workspace = stringValue(thread, QStringLiteral("cwd"));
            if (!workspace.isEmpty()) m_workspace = workspace;
            emit threadChanged(m_threadId, m_workspace);
        }
        return;
    }
    if (method == QStringLiteral("item/started")) {
        QJsonObject item = objectValue(params, QStringLiteral("item"));
        QString type = stringValue(item, QStringLiteral("type"));
        if (type == QStringLiteral("fileChange") || type == QStringLiteral("file_change")) {
            QString threadId = stringValue(params, QStringLiteral("threadId"));
            QString turnId = stringValue(params, QStringLiteral("turnId"));
            QString itemId = stringValue(params, QStringLiteral("itemId"));
            if (threadId.isEmpty()) threadId = stringValue(item, QStringLiteral("threadId"));
            if (turnId.isEmpty()) turnId = stringValue(item, QStringLiteral("turnId"));
            if (itemId.isEmpty()) itemId = stringValue(item, QStringLiteral("id"));
            QList<CodexFileChange> changes;
            for (QJsonValue const& value : item.value(QStringLiteral("changes")).toArray()) {
                if (!value.isObject()) continue;
                QJsonObject change = value.toObject();
                changes.push_back({
                    stringValue(change, QStringLiteral("path")).left(4096),
                    (stringValue(change, QStringLiteral("kind")).isEmpty()
                        ? stringValue(change, QStringLiteral("type"))
                        : stringValue(change, QStringLiteral("kind"))),
                    stringValue(change, QStringLiteral("diff")).left(131072)
                });
            }
            QString key = threadId + QLatin1Char('\x1f') + turnId + QLatin1Char('\x1f') + itemId;
            if (!m_fileChangesByItem.contains(key) && m_fileChangesByItem.size() >= 64) {
                m_fileChangesByItem.erase(m_fileChangesByItem.begin());
            }
            m_fileChangesByItem.insert(key, changes);
            for (auto const& pending : m_approvals.values()) {
                if (pending.kind != CodexApprovalKind::FileChange || pending.itemId != itemId) continue;
                if ((pending.threadId.isEmpty() || pending.threadId == threadId) &&
                    (pending.turnId.isEmpty() || pending.turnId == turnId)) {
                    m_approvals.updateChanges(pending.requestId, changes);
                    emit approvalRequested(*m_approvals.find(pending.requestId));
                }
            }
        }
        return;
    }
    if (method == QStringLiteral("turn/plan/updated")) {
        CodexPlanSnapshot update;
        if (parseCodexPlanSnapshot(method, params, update)) {
            if (!update.threadId.isEmpty()) m_plan.threadId = update.threadId;
            if (!update.turnId.isEmpty()) m_plan.turnId = update.turnId;
            m_plan.steps = update.steps;
            m_plan.explanation = update.explanation;
            emit planUpdated(m_plan);
        }
        return;
    }
    if (method == QStringLiteral("item/plan/delta")) {
        QString delta = stringValue(params, QStringLiteral("delta"));
        if (delta.isEmpty()) delta = stringValue(params, QStringLiteral("text"));
        QString threadId = stringValue(params, QStringLiteral("threadId"));
        QString turnId = stringValue(params, QStringLiteral("turnId"));
        QString itemId = stringValue(params, QStringLiteral("itemId"));
        QString key = threadId + QLatin1Char('\x1f') + turnId + QLatin1Char('\x1f') + itemId;
        if (!m_planDeltas.contains(key) && m_planDeltas.size() >= 64) {
            m_planDeltas.erase(m_planDeltas.begin());
        }
        QString& aggregate = m_planDeltas[key];
        if (aggregate.size() < 131072) aggregate += delta.left(131072 - aggregate.size());
        m_planDelta = aggregate;
        m_plan.finalText = aggregate;
        m_plan.threadId = threadId;
        m_plan.turnId = turnId;
        m_plan.itemId = itemId;
        emit planUpdated(m_plan);
        return;
    }
    if (method == QStringLiteral("item/agentMessage/delta")) {
        QString delta = stringValue(params, QStringLiteral("delta"));
        if (delta.isEmpty()) delta = stringValue(params, QStringLiteral("text"));
        m_finalMessage += delta;
        if (m_finalMessage.size() > 131072) m_finalMessage = m_finalMessage.right(131072);
        return;
    }
    if (method == QStringLiteral("item/completed")) {
        QJsonObject item = objectValue(params, QStringLiteral("item"));
        QString type = stringValue(item, QStringLiteral("type"));
        if (type == QStringLiteral("plan")) {
            CodexPlanSnapshot snapshot;
            if (parseCodexPlanSnapshot(method, params, snapshot)) {
                snapshot.final = true;
                m_plan = snapshot;
                QString key = snapshot.threadId + QLatin1Char('\x1f') + snapshot.turnId + QLatin1Char('\x1f') + snapshot.itemId;
                m_planDeltas.remove(key);
                emit finalPlanReady(m_plan);
            }
        }
        else if (type == QStringLiteral("agentMessage")) {
            QString phase = stringValue(item, QStringLiteral("phase"));
            QString text = stringValue(item, QStringLiteral("text"));
            if ((phase.isEmpty() || phase == QStringLiteral("final_answer")) && !text.isEmpty()) {
                m_finalMessage = text;
                emit finalMessageReady(m_finalMessage);
            }
        }
        return;
    }
    if (method == QStringLiteral("serverRequest/resolved")) {
        QJsonValue id = params.value(QStringLiteral("requestId"));
        if (id.isUndefined()) id = params.value(QStringLiteral("id"));
        if (!id.isUndefined()) {
            bool approval = m_approvals.take(id);
            auto it = m_userInputs.find(idKey(id));
            bool input = it != m_userInputs.end();
            if (input) m_userInputs.erase(it);
            if (approval) emit approvalResolved(id);
            if (input) emit userInputResolved(id);
            if (input && m_userInputs.isEmpty() && m_state == CodexServerState::NeedsInput) {
                setState(CodexServerState::Running);
            }
        }
        return;
    }
    if (method == QStringLiteral("turn/completed")) {
        QString status = stringValue(params, QStringLiteral("status"));
        if (status.isEmpty()) status = stringValue(objectValue(params, QStringLiteral("turn")), QStringLiteral("status"));
        bool success = status == QStringLiteral("completed") || status == QStringLiteral("succeeded");
        QString completedTurnId = m_turnId;
        cancelPendingRequests();
        m_turnId.clear();
        setState(success ? CodexServerState::Ready : CodexServerState::Blocked);
        emit turnFinished(completedTurnId, success);
        return;
    }
    if (method == QStringLiteral("thread/closed")) {
        cancelPendingRequests();
        m_threadId.clear();
        m_turnId.clear();
        setState(CodexServerState::Ready);
    }
}

void CodexAppServerClient::failClosed(QString const& reason) {
    if (m_state == CodexServerState::Blocked && m_process == nullptr) return;
    cancelPendingRequests();
    m_callbacks.clear();
    setState(CodexServerState::Blocked);
    emit protocolError(reason);
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) m_process->terminate();
}
