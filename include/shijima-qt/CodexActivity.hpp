#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

// The notification contract is intentionally small.  It mirrors the fields
// currently documented by Codex without retaining user input in the runtime.
enum class CodexActivityState {
    Running,
    NeedsInput,
    Ready,
    Blocked,
};

struct CodexActivity {
    QString type;
    QString threadId;
    QString turnId;
    QString cwd;
    QStringList inputMessages;
    QString lastAssistantMessage;
    CodexActivityState state = CodexActivityState::Ready;
};

constexpr qsizetype kCodexNotifyMaxBytes = 256 * 1024;

QString codexActivityStateName(CodexActivityState state);

// Parse one Codex notify payload.  Unknown event types are valid and return
// recognized=false so the CLI can exit successfully without touching the UI.
bool codexActivityFromJson(QJsonObject const& object, CodexActivity &activity,
    bool *recognized = nullptr, QString *errorMessage = nullptr);

QJsonObject codexActivityToJson(CodexActivity const& activity,
    bool includePrivateFields = false);

// Keep excerpts bounded by Unicode grapheme clusters.  This avoids splitting
// emoji, combining marks, or surrogate pairs while still keeping the UI small.
QString truncateCodexGraphemes(QString const& text, int maxGraphemes);
