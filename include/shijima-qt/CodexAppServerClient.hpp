#pragma once

#include "shijima-qt/CodexAppServerModels.hpp"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

class CodexApprovalStore {
public:
    static constexpr int kMaxPending = 16;

    bool insert(CodexApprovalRequest request);
    bool contains(QJsonValue const& id) const;
    bool take(QJsonValue const& id, CodexApprovalRequest* request = nullptr);
    CodexApprovalRequest const* find(QJsonValue const& id) const;
    bool updateChanges(QJsonValue const& id, QList<CodexFileChange> changes);
    QList<CodexApprovalRequest> values() const;
    void clear();
    int size() const { return m_requests.size(); }

private:
    QHash<QString, CodexApprovalRequest> m_requests;
    quint64 m_nextSequence = 1;
};

class CodexAppServerClient final : public QObject {
    Q_OBJECT
public:
    explicit CodexAppServerClient(QObject* parent = nullptr);
    ~CodexAppServerClient() override;

    CodexAppServerClient(CodexAppServerClient const&) = delete;
    CodexAppServerClient& operator=(CodexAppServerClient const&) = delete;

    CodexServerState state() const { return m_state; }
    QString executable() const { return m_executable; }
    void setExecutable(QString path);
    QString threadId() const { return m_threadId; }
    QString turnId() const { return m_turnId; }
    QString workspace() const { return m_workspace; }
    bool planSupported() const { return m_planSupported; }
    QStringList availableModes() const { return m_availableModes; }
    QList<CodexApprovalRequest> pendingApprovals() const;
    CodexPlanSnapshot planSnapshot() const { return m_plan; }
    QString finalMessage() const { return m_finalMessage; }

    // Starts one explicitly requested app-server process. No automatic
    // restart is attempted after a crash or protocol violation.
    bool start();
    void stop();
    void startNewThread(QString const& cwd = {});
    void resumeThread(QString const& threadId);
    void startTurn(QString const& text, bool planMode = false);
    void steerTurn(QString const& text);
    void interruptTurn();

    bool resolveApproval(QJsonValue const& requestId,
        CodexApprovalDecision decision);
    bool resolveUserInput(QJsonValue const& requestId,
        QJsonObject const& answers);
    void cancelPendingRequests();

signals:
    void stateChanged(CodexServerState state);
    void diagnostic(QString const& message);
    void protocolError(QString const& message);
    void threadChanged(QString const& threadId, QString const& workspace);
    void approvalRequested(CodexApprovalRequest const& request);
    void approvalResolved(QJsonValue const& requestId);
    void userInputRequested(CodexUserInputRequest const& request);
    void userInputResolved(QJsonValue const& requestId);
    void planUpdated(CodexPlanSnapshot const& snapshot);
    void finalPlanReady(CodexPlanSnapshot const& snapshot);
    void finalMessageReady(QString const& text);
    void turnFinished(QString const& turnId, bool success);

private slots:
    void readStdout();
    void readStderr();
    void processFinished(int exitCode, QProcess::ExitStatus status);
    void processError(QProcess::ProcessError error);

private:
    void setState(CodexServerState state);
    void sendRequest(QString const& method, QJsonObject const& params,
        std::function<void(QJsonValue const&, QJsonValue const&, QJsonObject const&)> callback = {});
    void sendNotification(QString const& method, QJsonObject const& params);
    void handleMessage(QByteArray const& line);
    void handleNotification(QString const& method, QJsonObject const& params);
    void handleRequest(QJsonValue const& id, QString const& method,
        QJsonObject const& params);
    void failClosed(QString const& reason);
    void writeJson(QByteArray const& payload);

    QProcess* m_process = nullptr;
    QString m_executable;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    QHash<QString, std::function<void(QJsonValue const&, QJsonValue const&, QJsonObject const&)>> m_callbacks;
    quint64 m_nextRequestId = 1;
    CodexServerState m_state = CodexServerState::Stopped;
    CodexApprovalStore m_approvals;
    QHash<QString, CodexUserInputRequest> m_userInputs;
    quint64 m_connectionGeneration = 0;
    QString m_threadId;
    QString m_turnId;
    QString m_workspace;
    QString m_finalMessage;
    CodexPlanSnapshot m_plan;
    QString m_planDelta;
    QHash<QString, QString> m_planDeltas;
    QHash<QString, QList<CodexFileChange>> m_fileChangesByItem;
    QStringList m_availableModes;
    bool m_planSupported = false;
    bool m_configRequirementsAllowed = true;
    QJsonObject m_defaultPreset;
    QJsonObject m_planPreset;
    bool m_initialized = false;
    bool m_stopping = false;
};

Q_DECLARE_METATYPE(CodexServerState)
Q_DECLARE_METATYPE(CodexApprovalRequest)
Q_DECLARE_METATYPE(CodexUserInputRequest)
Q_DECLARE_METATYPE(CodexPlanSnapshot)
