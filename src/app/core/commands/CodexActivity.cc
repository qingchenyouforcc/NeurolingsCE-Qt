#include "shijima-qt/CodexActivity.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTextBoundaryFinder>
#include <QVector>

namespace {

constexpr int kMaxMessages = 128;
constexpr qsizetype kMaxStringBytes = 128 * 1024;

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

    if (maxRetainedGraphemes == 1) {
        result.text = normalized.left(boundaries.at(1)) + QStringLiteral("…");
        result.retainedGraphemes = 1;
        result.truncated = true;
        return result;
    }

    int headCount = qMax(1, static_cast<int>(
        (static_cast<qint64>(maxRetainedGraphemes) * 4 + 6) / 7));
    headCount = qMin(headCount, maxRetainedGraphemes - 1);
    int tailCount = maxRetainedGraphemes - headCount;
    tailCount = qMin(tailCount, graphemeCount - headCount);
    headCount = qMin(headCount, graphemeCount - tailCount);

    QString head = normalized.left(boundaries.at(headCount)).trimmed();
    QString tail = normalized.mid(boundaries.at(graphemeCount - tailCount)).trimmed();
    result.text = head + QStringLiteral("\n…\n") + tail;
    result.retainedGraphemes = headCount + tailCount;
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
