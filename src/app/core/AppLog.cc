// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include "shijima-qt/AppLog.hpp"

#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <string>
#include <QString>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThread>

namespace {

std::mutex g_logMutex;
QFile *g_logFile = nullptr;
QString g_logDirectoryPath;
QtMessageHandler g_previousHandler = nullptr;
thread_local bool g_inMessageHandler = false;
bool g_mirrorToStderr = false;
AppLog::Level g_minimumLevel = AppLog::Level::Info;

#ifndef NEUROLINGSCE_VERSION
#define NEUROLINGSCE_VERSION "unknown"
#endif

QString applicationNameForFileSystem() {
    // Build filesystem-safe names so per-session log files stay portable across
    // platforms and shells.
    QString appName = QCoreApplication::applicationName().trimmed();
    if (appName.isEmpty()) {
        appName = QStringLiteral("neurolingsce");
    }
    appName = appName.toLower();
    for (QChar &ch : appName) {
        if (!ch.isLetterOrNumber()) {
            ch = '-';
        }
    }
    return appName;
}

QString sessionTimestampForFileName() {
    return QDateTime::currentDateTime().toString("HH-mm-ss-zzz");
}

QString dateFolderName() {
    return QDate::currentDate().toString("yyyy-MM-dd");
}

QString preferredLogRootDirectory(QCoreApplication *app) {
    Q_UNUSED(app);

#ifdef _WIN32
    // On Windows, prefer LOCALAPPDATA so logs stay in the conventional user
    // profile location even when Qt's writable path resolves elsewhere.
    QString localAppData = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("LOCALAPPDATA")).trimmed();
    if (!localAppData.isEmpty()) {
        return QDir(localAppData).filePath(QStringLiteral("NeurolingsCE/log"));
    }

    QString appLocalData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation).trimmed();
    if (!appLocalData.isEmpty()) {
        return QDir(appLocalData).filePath(QStringLiteral("log"));
    }
#else
    QString appLocalData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation).trimmed();
    if (!appLocalData.isEmpty()) {
        return QDir(appLocalData).filePath(QStringLiteral("log"));
    }
#endif

    return QDir::home().filePath(QStringLiteral(".neurolingsce/log"));
}

bool tryOpenLogFileAtRoot(QString const& rootDirectoryPath) {
    QDir rootDir(rootDirectoryPath);
    QString datedDirectoryPath = rootDir.filePath(dateFolderName());
    // Keep logs grouped by date so crash investigations and user support
    // remain easy to navigate.
    if (!rootDir.mkpath(dateFolderName())) {
        return false;
    }

    QString fileName = QStringLiteral("%1-%2.log").arg(
        applicationNameForFileSystem(), sessionTimestampForFileName());
    auto *file = new QFile(QDir(datedDirectoryPath).filePath(fileName));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete file;
        return false;
    }

    g_logFile = file;
    g_logDirectoryPath = datedDirectoryPath;
    return true;
}

int severity(AppLog::Level level) {
    return static_cast<int>(level);
}

bool shouldWrite(AppLog::Level level) {
    return severity(level) >= severity(g_minimumLevel);
}

QString normalizedLevel(QString value) {
    return value.trimmed().toLower();
}

bool parseLevel(QString value, AppLog::Level &level) {
    value = normalizedLevel(value);
    if (value.isEmpty()) {
        return false;
    }
    if (value == QStringLiteral("debug")) {
        level = AppLog::Level::Debug;
        return true;
    }
    if (value == QStringLiteral("info")) {
        level = AppLog::Level::Info;
        return true;
    }
    if (value == QStringLiteral("warning") || value == QStringLiteral("warn")) {
        level = AppLog::Level::Warning;
        return true;
    }
    if (value == QStringLiteral("error")) {
        level = AppLog::Level::Error;
        return true;
    }
    if (value == QStringLiteral("critical") || value == QStringLiteral("fatal")) {
        level = AppLog::Level::Critical;
        return true;
    }
    return false;
}

char const *levelName(AppLog::Level level) {
    switch (level) {
        case AppLog::Level::Debug:
            return "DEBUG";
        case AppLog::Level::Info:
            return "INFO";
        case AppLog::Level::Warning:
            return "WARN";
        case AppLog::Level::Error:
            return "ERROR";
        case AppLog::Level::Critical:
            return "CRITICAL";
    }
    return "INFO";
}

AppLog::Level fromQtType(QtMsgType type) {
    switch (type) {
        case QtDebugMsg:
            return AppLog::Level::Debug;
        case QtInfoMsg:
            return AppLog::Level::Info;
        case QtWarningMsg:
            return AppLog::Level::Warning;
        case QtCriticalMsg:
            return AppLog::Level::Error;
        case QtFatalMsg:
            return AppLog::Level::Critical;
    }
    return AppLog::Level::Info;
}

QString formatLine(AppLog::Level level, char const *category, QString const& message,
    char const *file, int line, char const *function)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString location;
    if (file != nullptr && *file != 0) {
        location = QFileInfo(QString::fromUtf8(file)).fileName();
        if (line > 0) {
            location += ":" + QString::number(line);
        }
    }
    else if (function != nullptr && *function != 0) {
        location = QString::fromUtf8(function);
    }
    else {
        location = "?";
    }

    // Include timestamp, severity, category, thread id, and source location in
    // a single line so both file logs and stderr mirrors are easy to grep.
    QString formatted = QStringLiteral("[%1] [%2] [%3] [tid:%4] %5")
        .arg(timestamp,
            QString::fromUtf8(::levelName(level)),
            QString::fromUtf8(category != nullptr ? category : "app"),
            QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16),
            message);
    if (!location.isEmpty()) {
        formatted += QStringLiteral(" (%1)").arg(location);
    }
    return formatted;
}

void writeLineUnlocked(QString const& line) {
    QByteArray utf8 = line.toUtf8();
    utf8.append('\n');

    if (g_logFile != nullptr && g_logFile->isOpen()) {
        g_logFile->write(utf8);
        g_logFile->flush();
    }
    else {
        std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
        std::fflush(stderr);
        return;
    }

    if (g_mirrorToStderr) {
        std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
        std::fflush(stderr);
    }
}

void appMessageHandler(QtMsgType type, QMessageLogContext const& context,
    QString const& message)
{
    if (g_inMessageHandler) {
        // Re-entrancy guard: if logging itself triggers Qt logging, fall back to
        // the previous handler instead of recursing forever.
        if (g_previousHandler != nullptr) {
            g_previousHandler(type, context, message);
        }
        return;
    }

    g_inMessageHandler = true;
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        AppLog::Level level = fromQtType(type);
        if (shouldWrite(level)) {
            QString line = formatLine(level, context.category, message,
                context.file, context.line, context.function);
            writeLineUnlocked(line);
        }
    }
    g_inMessageHandler = false;

    if (type == QtFatalMsg) {
        std::abort();
    }
}

}

namespace AppLog {

void initialize(QCoreApplication *app) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto environment = QProcessEnvironment::systemEnvironment();
    g_mirrorToStderr = environment.value("NEUROLINGSCE_LOG_STDERR") == "1";

    QString configuredLevel = environment.value("NEUROLINGSCE_LOG_LEVEL");
    bool invalidLevel = false;
    if (!configuredLevel.trimmed().isEmpty()) {
        AppLog::Level parsedLevel = Level::Info;
        if (parseLevel(configuredLevel, parsedLevel)) {
            g_minimumLevel = parsedLevel;
        }
        else {
            invalidLevel = true;
            g_minimumLevel = Level::Info;
        }
    }

    if (g_logFile == nullptr) {
        // Try the normal per-user log directory first, then fall back to temp
        // so startup still has somewhere to write diagnostics.
        QString preferredRoot = preferredLogRootDirectory(app);
        if (!tryOpenLogFileAtRoot(preferredRoot)) {
            QString fallbackRoot = QDir(QDir::tempPath())
                .filePath(QStringLiteral("NeurolingsCE/log"));
            if (fallbackRoot != preferredRoot) {
                tryOpenLogFileAtRoot(fallbackRoot);
            }
        }
    }

    if (g_previousHandler == nullptr) {
        g_previousHandler = qInstallMessageHandler(appMessageHandler);
    }

    if (invalidLevel) {
        writeLineUnlocked(formatLine(Level::Warning, "app",
            QStringLiteral("Invalid NEUROLINGSCE_LOG_LEVEL=\"%1\"; using info")
                .arg(configuredLevel),
            __FILE__, __LINE__, __func__));
    }
    writeLineUnlocked(formatLine(Level::Info, "app",
        QStringLiteral("Logging initialized. app=%1 version=%2 min_level=%3 stderr=%4 session_log=%5")
            .arg(QCoreApplication::applicationName(),
                QStringLiteral(NEUROLINGSCE_VERSION),
                QString::fromUtf8(levelName(g_minimumLevel)),
                g_mirrorToStderr ? QStringLiteral("1") : QStringLiteral("0"),
                sessionLogPath()),
        __FILE__, __LINE__, __func__));
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (shouldWrite(Level::Info)) {
        writeLineUnlocked(formatLine(Level::Info, "app", QStringLiteral("Logging shutdown"),
            __FILE__, __LINE__, __func__));
    }
    if (g_previousHandler != nullptr) {
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
    }
    if (g_logFile != nullptr) {
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
    g_logDirectoryPath = {};
}

void write(Level level, char const *category, std::string const& message,
    char const *file, int line, char const *function)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!shouldWrite(level)) {
        return;
    }
    writeLineUnlocked(formatLine(level, category,
        QString::fromUtf8(message.c_str()), file, line, function));
}

void writeCrash(char const *category, std::string const& message) {
    write(Level::Critical, category, message, nullptr, 0, nullptr);
}

bool isEnabled(Level level) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    return shouldWrite(level);
}

Level minimumLevel() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_minimumLevel;
}

char const *levelName(Level level) {
    return ::levelName(level);
}

QString sessionLogPath() {
    if (g_logFile != nullptr) {
        return g_logFile->fileName();
    }
    return {};
}

QString sessionLogDirectoryPath() {
    return g_logDirectoryPath;
}

Line::Line(Level level, char const *category, char const *file, int line,
    char const *function)
    : m_level(level), m_category(category), m_file(file), m_line(line),
      m_function(function)
{
}

Line::~Line() {
    AppLog::write(m_level, m_category, m_stream.str(), m_file, m_line, m_function);
}

std::ostringstream& Line::stream() {
    return m_stream;
}

}
