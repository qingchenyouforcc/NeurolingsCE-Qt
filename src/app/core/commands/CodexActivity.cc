#include "shijima-qt/CodexActivity.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTextBoundaryFinder>

namespace {

constexpr int kMaxMessages = 128;
constexpr qsizetype kMaxStringBytes = 128 * 1024;

bool readOptionalString(QJsonObject const& object, QString const& key,
    QString &target, QString *errorMessage)
{
    auto value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        target.clear();
        return true;
    }
    if (!value.isString()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("%1 must be a string").arg(key);
        }
        return false;
    }
    target = value.toString();
    if (target.toUtf8().size() > kMaxStringBytes) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("%1 is too large").arg(key);
        }
        return false;
    }
    return true;
}

}

QString codexActivityStateName(CodexActivityState state) {
    switch (state) {
        case CodexActivityState::Running:
            return QStringLiteral("running");
        case CodexActivityState::NeedsInput:
            return QStringLiteral("needs_input");
        case CodexActivityState::Ready:
            return QStringLiteral("ready");
        case CodexActivityState::Blocked:
            return QStringLiteral("blocked");
    }
    return QStringLiteral("ready");
}

bool codexActivityFromJson(QJsonObject const& object, CodexActivity &activity,
    bool *recognized, QString *errorMessage)
{
    if (recognized != nullptr) {
        *recognized = false;
    }
    activity = {};

    if (QJsonDocument(object).toJson(QJsonDocument::Compact).size() >
        kCodexNotifyMaxBytes)
    {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Codex notification JSON is too large");
        }
        return false;
    }

    auto typeValue = object.value(QStringLiteral("type"));
    if (!typeValue.isString() || typeValue.toString().trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("type must be a non-empty string");
        }
        return false;
    }
    activity.type = typeValue.toString();

    // Codex may add event types over time.  They are deliberately ignored so
    // installing a newer notify callback never breaks an older companion.
    if (activity.type != QStringLiteral("agent-turn-complete")) {
        return true;
    }
    if (recognized != nullptr) {
        *recognized = true;
    }

    if (!readOptionalString(object, QStringLiteral("thread-id"),
        activity.threadId, errorMessage) ||
        !readOptionalString(object, QStringLiteral("turn-id"),
            activity.turnId, errorMessage) ||
        !readOptionalString(object, QStringLiteral("cwd"),
            activity.cwd, errorMessage) ||
        !readOptionalString(object, QStringLiteral("last-assistant-message"),
            activity.lastAssistantMessage, errorMessage))
    {
        return false;
    }

    auto messagesValue = object.value(QStringLiteral("input-messages"));
    if (!messagesValue.isUndefined() && !messagesValue.isNull()) {
        if (!messagesValue.isArray()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("input-messages must be an array");
            }
            return false;
        }
        auto messages = messagesValue.toArray();
        if (messages.size() > kMaxMessages) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("input-messages contains too many entries");
            }
            return false;
        }
        for (auto const& message : messages) {
            if (!message.isString() && !message.isObject()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("input-messages entries must be strings or objects");
                }
                return false;
            }
            // Keep the field available to callers that need it, but never use
            // it for a bubble and never persist it in settings or logs.
            if (message.isString()) {
                if (message.toString().toUtf8().size() > kMaxStringBytes) {
                    if (errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("input-messages entry is too large");
                    }
                    return false;
                }
                activity.inputMessages.append(message.toString());
            }
        }
    }
    activity.state = CodexActivityState::Ready;
    return true;
}

QJsonObject codexActivityToJson(CodexActivity const& activity,
    bool includePrivateFields)
{
    QJsonObject object {
        { QStringLiteral("type"), activity.type },
        { QStringLiteral("state"), codexActivityStateName(activity.state) },
    };
    if (!activity.lastAssistantMessage.isEmpty()) {
        object.insert(QStringLiteral("last-assistant-message"),
            activity.lastAssistantMessage);
    }
    if (includePrivateFields) {
        if (!activity.threadId.isEmpty()) {
            object.insert(QStringLiteral("thread-id"), activity.threadId);
        }
        if (!activity.turnId.isEmpty()) {
            object.insert(QStringLiteral("turn-id"), activity.turnId);
        }
        if (!activity.cwd.isEmpty()) {
            object.insert(QStringLiteral("cwd"), activity.cwd);
        }
    }
    return object;
}

QString truncateCodexGraphemes(QString const& text, int maxGraphemes) {
    if (maxGraphemes <= 0 || text.isEmpty()) {
        return {};
    }
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0;
    int previousBoundary = 0;
    int end = 0;
    while ((end = finder.toNextBoundary()) >= 0) {
        ++count;
        if (count > maxGraphemes) {
            QString result = text.left(previousBoundary);
            return result.trimmed() + QStringLiteral("…");
        }
        previousBoundary = end;
    }
    return text;
}
