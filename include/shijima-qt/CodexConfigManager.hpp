#pragma once

#include <QString>
#include <QStringList>

struct CodexConfigResult {
    bool ok = false;
    bool changed = false;
    bool conflict = false;
    bool bridgedExistingNotify = false;
    QString path;
    QString backupPath;
    QString snippet;
    QString error;
};

QString codexConfigPath();
QString codexNotifyCommand(QString const& executablePath);
bool loadCodexForwardNotifyCommand(QString const& path,
    QStringList &arguments, QString *error = nullptr);

CodexConfigResult enableCodexNotify(QString const& path,
    QString const& executablePath);
CodexConfigResult disableCodexNotify(QString const& path);
