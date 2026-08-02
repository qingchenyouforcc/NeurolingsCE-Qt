#include "shijima-qt/CodexConfigManager.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

namespace {

QString beginMarker() {
    return QStringLiteral("# BEGIN NeurolingsCE Codex notify");
}

QString endMarker() {
    return QStringLiteral("# END NeurolingsCE Codex notify");
}

QString previousNotifyPrefix() {
    return QStringLiteral("# NeurolingsCE previous-notify-base64: ");
}

QString escapeToml(QString value) {
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    value.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    return value;
}

QString managedBlock(QString const& executablePath,
    QString const& previousNotifyLine = {})
{
    QString block = beginMarker() + QLatin1Char('\n');
    if (!previousNotifyLine.isEmpty()) {
        block += previousNotifyPrefix() + QString::fromLatin1(
            previousNotifyLine.toUtf8().toBase64()) + QLatin1Char('\n');
    }
    block += QStringLiteral("notify = [\"") +
        escapeToml(QFileInfo(executablePath).absoluteFilePath()) +
        QStringLiteral("\", \"--codex-notify\"]\n") + endMarker() +
        QLatin1Char('\n');
    return block;
}

QString unescapeToml(QString value) {
    QString result;
    result.reserve(value.size());
    bool escaped = false;
    for (QChar const character : value) {
        if (!escaped) {
            if (character == QLatin1Char('\\')) {
                escaped = true;
            }
            else {
                result += character;
            }
            continue;
        }

        switch (character.unicode()) {
        case 'b': result += QLatin1Char('\b'); break;
        case 'f': result += QLatin1Char('\f'); break;
        case 'n': result += QLatin1Char('\n'); break;
        case 'r': result += QLatin1Char('\r'); break;
        case 't': result += QLatin1Char('\t'); break;
        case '\\': result += QLatin1Char('\\'); break;
        case '"': result += QLatin1Char('"'); break;
        default:
            // Keep unknown escapes intact rather than guessing at TOML syntax
            // that this migration helper does not need to interpret.
            result += QLatin1Char('\\');
            result += character;
            break;
        }
        escaped = false;
    }
    if (escaped) {
        result += QLatin1Char('\\');
    }
    return result;
}

bool readFile(QString const& path, QString &content, QString &error) {
    QFile file(path);
    if (!file.exists()) {
        content.clear();
        return true;
    }
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        error = QStringLiteral("Could not read Codex configuration: %1")
            .arg(file.errorString());
        return false;
    }
    content = QString::fromUtf8(file.readAll());
    return true;
}

bool writeAtomically(QString const& path, QString const& content,
    CodexConfigResult &result)
{
    QFileInfo info(path);
    if (!info.absoluteDir().exists() &&
        !QDir().mkpath(info.absolutePath()))
    {
        result.error = QStringLiteral("Could not create Codex configuration directory");
        return false;
    }

    if (QFile::exists(path)) {
        QString backupStem = path + QStringLiteral(".bak.") +
            QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
        result.backupPath = backupStem;
        int suffix = 1;
        while (QFile::exists(result.backupPath)) {
            result.backupPath = backupStem + QStringLiteral("-%1").arg(suffix++);
        }
        if (!QFile::copy(path, result.backupPath)) {
            result.error = QStringLiteral("Could not create a backup of Codex configuration");
            return false;
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.error = QStringLiteral("Could not open Codex configuration for writing: %1")
            .arg(file.errorString());
        return false;
    }
    auto bytes = content.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        result.error = QStringLiteral("Could not atomically update Codex configuration: %1")
            .arg(file.errorString());
        return false;
    }
    result.changed = true;
    return true;
}

bool findManagedBlock(QString const& content, int &start, int &end) {
    start = content.indexOf(beginMarker());
    if (start < 0) {
        end = -1;
        return false;
    }
    end = content.indexOf(endMarker(), start + beginMarker().size());
    if (end < 0) {
        return false;
    }
    end += endMarker().size();
    if (end < content.size() && content.at(end) == QLatin1Char('\r')) {
        ++end;
    }
    if (end < content.size() && content.at(end) == QLatin1Char('\n')) {
        ++end;
    }
    return true;
}

bool previousNotifyFromManagedBlock(QString const& content, int start, int end,
    QString &line, QString &error)
{
    line.clear();
    if (start < 0 || end <= start) {
        return true;
    }
    static const QRegularExpression pattern(QStringLiteral(
        "(?m)^# NeurolingsCE previous-notify-base64: ([A-Za-z0-9+/=]+)\\r?$"));
    auto match = pattern.match(content, start);
    if (!match.hasMatch() || match.capturedStart() >= end) {
        return true;
    }
    QByteArray decoded = QByteArray::fromBase64(match.captured(1).toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.isEmpty()) {
        error = QStringLiteral("NeurolingsCE's managed Codex block contains invalid forwarding metadata");
        return false;
    }
    line = QString::fromUtf8(decoded);
    return true;
}

bool parseNotifyLine(QString line, QStringList &arguments) {
    arguments.clear();
    while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r'))) {
        line.chop(1);
    }
    int equals = line.indexOf(QLatin1Char('='));
    if (equals < 0) {
        return false;
    }

    QJsonParseError parseError;
    auto document = QJsonDocument::fromJson(
        line.mid(equals + 1).trimmed().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray() ||
        document.array().isEmpty())
    {
        return false;
    }
    for (auto const& value : document.array()) {
        if (!value.isString()) {
            arguments.clear();
            return false;
        }
        arguments.append(value.toString());
    }
    return true;
}

bool isCodexComputerUseNotify(QStringList const& arguments) {
    if (arguments.size() != 2 ||
        arguments.at(1) != QStringLiteral("turn-ended"))
    {
        return false;
    }
    // Codex Desktop writes a Windows executable path into config.toml. Keep
    // recognition deterministic when this parser is exercised on Linux or
    // macOS, where QFileInfo does not treat backslashes as separators.
    QString executablePath = arguments.front();
    executablePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString fileName = QFileInfo(executablePath).fileName();
    return fileName.compare(QStringLiteral("codex-computer-use.exe"),
        Qt::CaseInsensitive) == 0 ||
        fileName.compare(QStringLiteral("codex-computer-use"),
            Qt::CaseInsensitive) == 0;
}

struct NotifyLine {
    int start = -1;
    int end = -1;
    QString text;
};

QList<NotifyLine> externalNotifyLines(QString const& content,
    int managedStart, int managedEnd, int legacyStart, int legacyEnd)
{
    QList<NotifyLine> lines;
    static const QRegularExpression pattern(QStringLiteral(
        "(?m)^[\\t ]*notify[\\t ]*=[^\\r\\n]*(?:\\r\\n|\\n|$)"));
    auto matches = pattern.globalMatch(content);
    while (matches.hasNext()) {
        auto match = matches.next();
        int start = match.capturedStart();
        if ((managedStart >= 0 && start >= managedStart && start < managedEnd) ||
            (legacyStart >= 0 && start >= legacyStart && start < legacyEnd))
        {
            continue;
        }
        lines.append({ start, static_cast<int>(match.capturedEnd()),
            match.captured() });
    }
    return lines;
}

bool isNeurolingsCeCli(QString const& path) {
    QString fileName = QFileInfo(unescapeToml(path)).fileName();
    return fileName.compare(QStringLiteral("NeurolingsCE-cli.exe"),
        Qt::CaseInsensitive) == 0 ||
        fileName.compare(QStringLiteral("NeurolingsCE-cli"),
            Qt::CaseInsensitive) == 0;
}

bool findLegacyNeurolingsCEBlock(QString const& content,
    int managedStart, int managedEnd, int &start, int &end)
{
    static const QRegularExpression legacyPattern(
        QStringLiteral("(?m)^[\\t ]*notify[\\t ]*=[\\t ]*\\[[\\t ]*\""
            "((?:\\\\.|[^\"\\\\])*)\"[\\t ]*,[\\t ]*"
            "\"--codex-notify\"[\\t ]*\\][\\t ]*(?:\\r?\\n|$)"));
    auto matches = legacyPattern.globalMatch(content);
    while (matches.hasNext()) {
        auto match = matches.next();
        int matchStart = match.capturedStart();
        int matchEnd = match.capturedEnd();
        if (managedStart >= 0 && matchStart >= managedStart &&
            matchStart < managedEnd)
        {
            continue;
        }
        if (!isNeurolingsCeCli(match.captured(1))) {
            continue;
        }
        start = matchStart;
        end = matchEnd;
        return true;
    }
    start = -1;
    end = -1;
    return false;
}

}

QString codexConfigPath() {
    QString home = qEnvironmentVariable("CODEX_HOME").trimmed();
    if (home.isEmpty()) {
        home = QDir::homePath() + QDir::separator() + QStringLiteral(".codex");
    }
    return QDir(home).absoluteFilePath(QStringLiteral("config.toml"));
}

QString codexNotifyCommand(QString const& executablePath) {
    return QStringLiteral("notify = [\"") +
        escapeToml(QFileInfo(executablePath).absoluteFilePath()) +
        QStringLiteral("\", \"--codex-notify\"]");
}

bool loadCodexForwardNotifyCommand(QString const& path,
    QStringList &arguments, QString *error)
{
    arguments.clear();
    QString content;
    QString readError;
    if (!readFile(QFileInfo(path).absoluteFilePath(), content, readError)) {
        if (error) {
            *error = readError;
        }
        return false;
    }
    int start = -1;
    int end = -1;
    if (!findManagedBlock(content, start, end)) {
        return true;
    }
    QString previousLine;
    QString metadataError;
    if (!previousNotifyFromManagedBlock(content, start, end, previousLine,
        metadataError))
    {
        if (error) {
            *error = metadataError;
        }
        return false;
    }
    if (previousLine.isEmpty()) {
        return true;
    }
    if (!parseNotifyLine(previousLine, arguments) ||
        !isCodexComputerUseNotify(arguments))
    {
        arguments.clear();
        if (error) {
            *error = QStringLiteral("NeurolingsCE's managed Codex block contains an unsupported forwarding command");
        }
        return false;
    }
    return true;
}

CodexConfigResult enableCodexNotify(QString const& path,
    QString const& executablePath)
{
    CodexConfigResult result;
    result.path = QFileInfo(path).absoluteFilePath();
    QFileInfo executableInfo(executablePath);
    if (!executableInfo.exists() || !executableInfo.isFile()) {
        result.snippet = codexNotifyCommand(executablePath);
        result.error = QStringLiteral("NeurolingsCE CLI executable was not found: %1")
            .arg(executableInfo.absoluteFilePath());
        return result;
    }
    QString content;
    if (!readFile(result.path, content, result.error)) {
        return result;
    }
    int start = -1;
    int end = -1;
    bool hasBlock = findManagedBlock(content, start, end);
    int legacyStart = -1;
    int legacyEnd = -1;
    bool hasLegacyBlock = findLegacyNeurolingsCEBlock(content,
        hasBlock ? start : -1, hasBlock ? end : -1, legacyStart, legacyEnd);
    auto externalLines = externalNotifyLines(content,
        hasBlock ? start : -1, hasBlock ? end : -1,
        hasLegacyBlock ? legacyStart : -1,
        hasLegacyBlock ? legacyEnd : -1);

    QString previousNotifyLine;
    if (hasBlock && !previousNotifyFromManagedBlock(content, start, end,
        previousNotifyLine, result.error))
    {
        return result;
    }

    bool canBridgeComputerUse = false;
    if (!hasBlock && !hasLegacyBlock && externalLines.size() == 1) {
        QStringList externalArguments;
        canBridgeComputerUse = parseNotifyLine(externalLines.front().text,
            externalArguments) && isCodexComputerUseNotify(externalArguments);
        if (canBridgeComputerUse) {
            previousNotifyLine = externalLines.front().text;
        }
    }
    if ((!externalLines.isEmpty() && !canBridgeComputerUse) ||
        externalLines.size() > 1)
    {
        result.conflict = true;
        result.snippet = codexNotifyCommand(executablePath);
        result.error = QStringLiteral("Codex config already contains a non-NeurolingsCE notify setting");
        return result;
    }

    QString updated;
    QString block = managedBlock(executablePath, previousNotifyLine);
    if (hasBlock) {
        updated = content;
        if (hasLegacyBlock) {
            updated.remove(legacyStart, legacyEnd - legacyStart);
            if (legacyStart < start) {
                int legacyLength = legacyEnd - legacyStart;
                start -= legacyLength;
                end -= legacyLength;
            }
        }
        updated.replace(start, end - start, block);
    }
    else if (hasLegacyBlock) {
        updated = content;
        // The old unmarked command was written by NeurolingsCE before managed
        // markers existed.  Keep an extra separator so disableCodexNotify()
        // restores the bytes that preceded that legacy line.
        QString replacement = block;
        if (legacyStart > 0) {
            replacement.prepend(QLatin1Char('\n'));
        }
        updated.replace(legacyStart, legacyEnd - legacyStart, replacement);
    }
    else if (canBridgeComputerUse) {
        updated = content;
        auto const& external = externalLines.front();
        updated.replace(external.start, external.end - external.start, block);
        result.bridgedExistingNotify = true;
    }
    else {
        updated = content;
        // Reserve one separator newline for the managed block.  The disable
        // path removes this exact separator, so files without a final newline
        // are restored byte-for-byte as well.
        if (!updated.isEmpty()) {
            updated += QLatin1Char('\n');
        }
        updated += block;
    }

    result.ok = true;
    if (updated == content) {
        return result;
    }
    if (!writeAtomically(result.path, updated, result)) {
        result.ok = false;
    }
    return result;
}

CodexConfigResult disableCodexNotify(QString const& path) {
    CodexConfigResult result;
    result.path = QFileInfo(path).absoluteFilePath();
    QString content;
    if (!readFile(result.path, content, result.error)) {
        return result;
    }
    int start = -1;
    int end = -1;
    if (!findManagedBlock(content, start, end)) {
        result.ok = true;
        return result;
    }

    QString previousNotifyLine;
    if (!previousNotifyFromManagedBlock(content, start, end,
        previousNotifyLine, result.error))
    {
        return result;
    }

    QString updated = content;
    if (!previousNotifyLine.isEmpty()) {
        updated.replace(start, end - start, previousNotifyLine);
        result.ok = true;
        if (!writeAtomically(result.path, updated, result)) {
            result.ok = false;
        }
        return result;
    }
    int removalStart = start;
    if (removalStart > 0 && content.at(removalStart - 1) == QLatin1Char('\n')) {
        --removalStart;
    }
    updated.remove(removalStart, end - removalStart);
    result.ok = true;
    if (updated == content) {
        return result;
    }
    if (!writeAtomically(result.path, updated, result)) {
        result.ok = false;
    }
    return result;
}
