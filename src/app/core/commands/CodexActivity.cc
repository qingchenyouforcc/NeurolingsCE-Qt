#include "shijima-qt/CodexActivity.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTextBoundaryFinder>
#include <QVector>

#include <initializer_list>

namespace {

constexpr int kMaxMessages = 128;
constexpr qsizetype kMaxStringBytes = 128 * 1024;

QString normalizedEventType(QString type) {
    type = type.trimmed().toLower();
    type.replace(QChar('_'), QChar('-'));
    return type;
}

bool isNewSessionEvent(QString const& type) {
    auto normalized = normalizedEventType(type);
    return normalized == QStringLiteral("session-title-updated") ||
        normalized == QStringLiteral("session-title-generated") ||
        normalized == QStringLiteral("session-title-changed") ||
        normalized == QStringLiteral("session/title/updated") ||
        normalized == QStringLiteral("session/title/changed") ||
        normalized == QStringLiteral("session/titlechanged") ||
        normalized == QStringLiteral("session-name-updated") ||
        normalized == QStringLiteral("session-name-changed") ||
        normalized == QStringLiteral("session/name/updated") ||
        normalized == QStringLiteral("session/name/changed") ||
        normalized == QStringLiteral("thread-title-updated") ||
        normalized == QStringLiteral("thread-title-changed") ||
        normalized == QStringLiteral("thread/title/updated") ||
        normalized == QStringLiteral("thread/title/changed") ||
        normalized == QStringLiteral("thread-name-updated") ||
        normalized == QStringLiteral("thread-name-changed") ||
        normalized == QStringLiteral("thread-name/updated") ||
        normalized == QStringLiteral("thread/name/changed") ||
        normalized == QStringLiteral("thread/name/updated") ||
        normalized == QStringLiteral("new-session") ||
        normalized == QStringLiteral("session-started");
}

QJsonValue findValue(QJsonObject const& object,
    std::initializer_list<QString> keys)
{
    for (auto const& key : keys) {
        auto value = object.value(key);
        if (!value.isUndefined() && !value.isNull()) {
            return value;
        }
    }
    // App-server shaped notifications sometimes wrap the update in params or
    // a thread/session object.  Only inspect known object containers; this
    // keeps parsing deterministic and avoids recursively walking untrusted
    // payloads.
    for (auto const& containerKey : { QStringLiteral("params"),
        QStringLiteral("session"), QStringLiteral("thread"),
        QStringLiteral("data") })
    {
        auto container = object.value(containerKey);
        if (!container.isObject()) {
            continue;
        }
        auto nested = container.toObject();
        for (auto const& key : keys) {
            auto value = nested.value(key);
            if (!value.isUndefined() && !value.isNull()) {
                return value;
            }
        }
    }
    return {};
}

bool readAliasedOptionalString(QJsonObject const& object,
    std::initializer_list<QString> keys, QString &target,
    QString *errorMessage)
{
    for (auto const& key : keys) {
        auto value = findValue(object, { key });
        if (value.isUndefined() || value.isNull()) {
            continue;
        }
        if (!value.isString()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("%1 must be a string")
                    .arg(key);
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
    target.clear();
    return true;
}

QVector<int> graphemeBoundaries(QString const& text) {
    QVector<int> boundaries { 0 };
    if (text.isEmpty()) {
        return boundaries;
    }

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    while (true) {
        int boundary = finder.toNextBoundary();
        if (boundary < 0 || boundary <= boundaries.constLast()) {
            break;
        }
        boundaries.append(boundary);
    }
    if (boundaries.constLast() != text.size()) {
        boundaries.append(text.size());
    }
    return boundaries;
}

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
    // JSON-RPC/app-server notifications call this field `method`; the
    // legacy Codex notify hook uses `type`. Accept both without recursively
    // walking arbitrary payload data.
    if (!typeValue.isString()) {
        auto methodValue = object.value(QStringLiteral("method"));
        if (methodValue.isString()) {
            typeValue = methodValue;
        }
    }
    if (!typeValue.isString() || typeValue.toString().trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("type must be a non-empty string");
        }
        return false;
    }
    activity.type = typeValue.toString();

    // Codex may add event types over time.  They are deliberately ignored so
    // installing a newer notify callback never breaks an older companion.
    bool const newSession = isNewSessionEvent(activity.type);
    if (activity.type != QStringLiteral("agent-turn-complete") &&
        !newSession)
    {
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

    if (newSession) {
        if (!readAliasedOptionalString(object,
            { QStringLiteral("title"), QStringLiteral("session-title"),
                QStringLiteral("sessionTitle"), QStringLiteral("thread-title"),
                QStringLiteral("threadTitle"), QStringLiteral("thread-name"),
                QStringLiteral("threadName"), QStringLiteral("session-name"),
                QStringLiteral("sessionName"), QStringLiteral("name") },
            activity.sessionTitle, errorMessage) ||
            activity.sessionTitle.trimmed().isEmpty())
        {
            if (errorMessage != nullptr && errorMessage->isEmpty()) {
                *errorMessage = QStringLiteral(
                    "new session notification requires a title");
            }
            return false;
        }
        if (!readAliasedOptionalString(object,
            { QStringLiteral("description"), QStringLiteral("summary"),
                QStringLiteral("session-description"),
                QStringLiteral("sessionDescription"),
                QStringLiteral("preview"), QStringLiteral("message"),
                QStringLiteral("body"), QStringLiteral("content") },
            activity.sessionDescription, errorMessage))
        {
            return false;
        }
        activity.isNewSession = true;
        // A few Codex builds send the description in the completion field.
        if (activity.sessionDescription.isEmpty()) {
            activity.sessionDescription = activity.lastAssistantMessage;
        }
    }
    else if (activity.type == QStringLiteral("agent-turn-complete")) {
        // Be tolerant of desktop builds that attach title metadata to the
        // completion event instead of sending a separate event.  Only treat
        // it as a new-session event when a non-empty title is present.
        QString title;
        if (!readAliasedOptionalString(object,
            { QStringLiteral("title"), QStringLiteral("session-title"),
                QStringLiteral("sessionTitle"), QStringLiteral("thread-title"),
                QStringLiteral("threadTitle"), QStringLiteral("thread-name"),
                QStringLiteral("threadName") }, title, errorMessage))
        {
            return false;
        }
        if (!title.trimmed().isEmpty()) {
            activity.sessionTitle = title;
            if (!readAliasedOptionalString(object,
                { QStringLiteral("description"), QStringLiteral("summary"),
                    QStringLiteral("preview"), QStringLiteral("message"),
                    QStringLiteral("body"), QStringLiteral("content") },
                activity.sessionDescription, errorMessage))
            {
                return false;
            }
            if (activity.sessionDescription.isEmpty()) {
                activity.sessionDescription = activity.lastAssistantMessage;
            }
            activity.isNewSession = true;
        }
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
    if (activity.isNewSession && activity.sessionDescription.isEmpty() &&
        !activity.inputMessages.isEmpty())
    {
        activity.sessionDescription = activity.inputMessages.join(
            QStringLiteral("\n"));
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
    if (!activity.sessionTitle.isEmpty()) {
        object.insert(QStringLiteral("title"), activity.sessionTitle);
    }
    if (!activity.sessionDescription.isEmpty()) {
        object.insert(QStringLiteral("description"), activity.sessionDescription);
    }
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

QString normalizeCodexBubbleText(QString const& text) {
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar('\r'), QChar('\n'));
    while (normalized.contains(QStringLiteral("\n\n\n"))) {
        normalized.replace(QStringLiteral("\n\n\n"), QStringLiteral("\n\n"));
    }
    return normalized.trimmed();
}

CodexTextExcerpt compactCodexBubbleSource(QString const& text,
    int maxRetainedGraphemes)
{
    CodexTextExcerpt result;
    QString normalized = normalizeCodexBubbleText(text);
    if (maxRetainedGraphemes <= 0 || normalized.isEmpty()) {
        return result;
    }

    auto boundaries = graphemeBoundaries(normalized);
    int graphemeCount = boundaries.size() - 1;
    if (graphemeCount <= maxRetainedGraphemes) {
        result.text = normalized;
        result.retainedGraphemes = graphemeCount;
        return result;
    }

    // Codex notify text is a completion summary.  Preserve that summary from
    // the beginning and make truncation a terminal marker; retaining a tail
    // here can expose tool protocol fragments (for example `::git-push{...}`)
    // without the surrounding context.  The UI formatter performs a second,
    // Markdown-aware prefix fit, so this coarse grapheme bound must not
    // re-introduce a head/tail excerpt.
    int retained = qMax(1, maxRetainedGraphemes);
    retained = qMin(retained, graphemeCount);
    result.text = normalized.left(boundaries.at(retained)).trimmed()
        + QStringLiteral("…");
    result.retainedGraphemes = retained;
    result.truncated = true;
    return result;
}

QString truncateCodexGraphemes(QString const& text, int maxGraphemes) {
    if (maxGraphemes <= 0 || text.isEmpty()) {
        return {};
    }
    auto boundaries = graphemeBoundaries(text);
    if (boundaries.size() - 1 <= maxGraphemes) {
        return text;
    }
    return text.left(boundaries.at(maxGraphemes)).trimmed() + QStringLiteral("…");
}
