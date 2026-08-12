#pragma once

// NeurolingsCE - Codex app-server protocol models
// Copyright (C) 2025 pixelomer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

enum class CodexServerState {
    Stopped,
    Starting,
    Initializing,
    Ready,
    Running,
    NeedsInput,
    Blocked,
    Stopping
};

enum class CodexApprovalKind {
    CommandExecution,
    FileChange,
    Network
};

enum class CodexApprovalDecision {
    Accept,
    AcceptForSession,
    Decline,
    Cancel
};

struct CodexCommandAction {
    QString type;
    QString command;
    QString description;
};

struct CodexFileChange {
    QString path;
    QString kind;
    QString diff;
};

struct CodexPlanStep {
    QString step;
    QString status;
};

struct CodexApprovalRequest {
    QJsonValue requestId;
    CodexApprovalKind kind = CodexApprovalKind::CommandExecution;
    QString threadId;
    QString turnId;
    QString itemId;
    QString reason;
    QString command;
    QString cwd;
    QList<CodexCommandAction> commandActions;
    QList<CodexFileChange> changes;
    QJsonObject networkContext;
    QStringList availableDecisions;
    quint64 connectionGeneration = 0;
    quint64 sequence = 0;
};

struct CodexPlanSnapshot {
    QString threadId;
    QString turnId;
    QString itemId;
    QString explanation;
    QString finalText;
    QList<CodexPlanStep> steps;
    bool final = false;
};

struct CodexUserInputOption {
    QString label;
    QString description;
};

struct CodexUserInputQuestion {
    QString id;
    QString header;
    QString question;
    QList<CodexUserInputOption> options;
    bool isOther = false;
    bool isSecret = false;
};

struct CodexUserInputRequest {
    QJsonValue requestId;
    QString threadId;
    QString turnId;
    QString itemId;
    QList<CodexUserInputQuestion> questions;
    int autoResolutionMs = 0;
    quint64 connectionGeneration = 0;
};

QString codexServerStateName(CodexServerState state);
QString codexApprovalKindName(CodexApprovalKind kind);
QString codexApprovalDecisionName(CodexApprovalDecision decision);
bool codexApprovalDecisionFromName(QString const& name,
    CodexApprovalDecision& decision);
QString codexApprovalRequestIdKey(QJsonValue const& id);
