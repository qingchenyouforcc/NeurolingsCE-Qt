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

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include "shijima-qt/AppLog.hpp"
#include <shijima/log.hpp>
#include "Platform/Platform.hpp"
#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/AssetLoader.hpp"
#include "shijima-qt/ShijimaLocalApi.hpp"
#include "shijima-qt/cli.hpp"
#include "ElaApplication.h"
#include <exception>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

QString const kCliRuntimeArgument = QStringLiteral("--neurolingsce-cli-runtime");
QString const kStartupLaunchArgument = QStringLiteral("--neurolingsce-startup");

QString normalizeBootstrapLanguage(QString code)
{
    code = code.trimmed();
    if (code.compare(QStringLiteral("zh_CN"), Qt::CaseInsensitive) == 0 ||
        code.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive))
    {
        return QStringLiteral("zh_CN");
    }
    return QStringLiteral("en");
}

struct BootstrapTranslators {
    QCoreApplication *app = nullptr;
    QTranslator own;
    QTranslator qt;
    bool ownInstalled = false;
    bool qtInstalled = false;

    ~BootstrapTranslators()
    {
        uninstall();
    }

    void install(QCoreApplication *application, QString const& language)
    {
        app = application;
        if (app == nullptr || language == QStringLiteral("en")) {
            return;
        }
        if (own.load(QStringLiteral("shijima-qt_") + language,
                QStringLiteral(":/i18n"))) {
            app->installTranslator(&own);
            ownInstalled = true;
        }
        if (qt.load(QStringLiteral("qt_") + language,
                QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            app->installTranslator(&qt);
            qtInstalled = true;
        }
    }

    void uninstall()
    {
        if (app == nullptr) {
            return;
        }
        if (qtInstalled) {
            app->removeTranslator(&qt);
            qtInstalled = false;
        }
        if (ownInstalled) {
            app->removeTranslator(&own);
            ownInstalled = false;
        }
    }
};

bool cliRuntimeMode(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == kCliRuntimeArgument) {
            return true;
        }
    }
    return false;
}

bool startupLaunchMode(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == kStartupLaunchArgument) {
            return true;
        }
    }
    return false;
}

void syncBundledSkillsForCurrentUser(QCoreApplication *app) {
#ifdef _WIN32
    if (app == nullptr) {
        return;
    }

    QString appRoot = app->applicationDirPath();
    QString installScript = QDir(appRoot)
        .filePath(QStringLiteral("neurolingsce-skill/scripts/install_to_codex_home.ps1"));
    if (!QFileInfo::exists(installScript)) {
        return;
    }

    QString program = QStringLiteral("powershell.exe");
    QStringList arguments {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        installScript,
        QStringLiteral("-AppRoot"),
        appRoot,
    };

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000)) {
        APP_LOG_WARN("startup") << "Could not start bundled skill install script";
        return;
    }
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished(3000);
        APP_LOG_WARN("startup") << "Bundled skill install script timed out";
        return;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        APP_LOG_WARN("startup") << "Bundled skill install script failed with exit code"
            << process.exitCode() << "stderr="
            << process.readAllStandardError().toStdString();
        return;
    }
    APP_LOG_INFO("startup") << "Bundled skill install script completed";
#else
    Q_UNUSED(app);
#endif
}

AppLog::Level shijimaLogLevel(uint16_t level) {
    if (level == SHIJIMA_LOG_EVERYTHING) {
        return AppLog::Level::Debug;
    }
    if ((level & SHIJIMA_LOG_WARNINGS) != 0) {
        return AppLog::Level::Warning;
    }
    return AppLog::Level::Debug;
}

void installShijimaEngineLogger() {
#ifdef SHIJIMA_LOGGING_ENABLED
    shijima::set_logger([](uint16_t level, std::string const& message) {
        AppLog::write(shijimaLogLevel(level), "engine", message, nullptr, 0, nullptr);
    });
    shijima::set_log_level(SHIJIMA_LOG_EVERYTHING);
    APP_LOG_INFO("startup") << "Shijima engine logging bridge installed";
#endif
}

#ifdef _WIN32
LONG WINAPI appUnhandledExceptionFilter(EXCEPTION_POINTERS *info) {
    std::ostringstream oss;
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        oss << "Unhandled SEH exception code=0x"
            << std::hex << std::uppercase
            << info->ExceptionRecord->ExceptionCode
            << " address=" << info->ExceptionRecord->ExceptionAddress;
    }
    else {
        oss << "Unhandled SEH exception with null EXCEPTION_POINTERS";
    }
    AppLog::writeCrash("crash", oss.str());
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void appTerminateHandler() {
    try {
        auto ex = std::current_exception();
        if (ex != nullptr) {
            std::rethrow_exception(ex);
        }
        AppLog::writeCrash("crash", "std::terminate called without an active exception");
    }
    catch (std::exception const& ex) {
        AppLog::writeCrash("crash", std::string("std::terminate: ") + ex.what());
    }
    catch (...) {
        AppLog::writeCrash("crash", "std::terminate: unknown exception");
    }
    std::abort();
}

}

int main(int argc, char **argv) {
    // The GUI binary also doubles as a CLI launcher in some workflows.
    // Keep that path isolated so we can use QCoreApplication without UI setup.
    if (shijimaShouldRunCli(argc, argv)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName(QStringLiteral(APP_NAME));
        AppLog::initialize(&app);
        installShijimaEngineLogger();
        APP_LOG_INFO("startup") << "CLI-in-GUI executable mode selected argc=" << argc
            << " version=\"" << NEUROLINGSCE_VERSION << "\"";
        std::set_terminate(appTerminateHandler);
#ifdef _WIN32
        SetUnhandledExceptionFilter(appUnhandledExceptionFilter);
#endif
        int ret = shijimaRunCli(argc, argv);
        APP_LOG_INFO("shutdown") << "CLI-in-GUI executable mode finished code=" << ret;
        AppLog::shutdown();
        return ret;
    }
    bool const startedForCli = cliRuntimeMode(argc, argv);
    bool const startedFromSystemStartup = startupLaunchMode(argc, argv);
    Platform::initialize(argc, argv);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral(APP_NAME));
    app.setApplicationDisplayName(QStringLiteral(APP_DISPLAY_NAME));
    app.setProperty("neurolingsce.cliRuntime", startedForCli);
    app.setProperty("neurolingsce.startupLaunch", startedFromSystemStartup);
    if (startedForCli) {
        app.setQuitOnLastWindowClosed(false);
    }
    BootstrapTranslators bootstrapTranslators;
    if (!startedForCli) {
        QSettings settings(QStringLiteral("pixelomer"),
            QStringLiteral("Shijima-Qt"));
        QString language = settings.contains(QStringLiteral("language"))
            ? normalizeBootstrapLanguage(settings.value(
                QStringLiteral("language")).toString())
            : normalizeBootstrapLanguage(QLocale::system().name());
        bootstrapTranslators.install(&app, language);
    }
    AppLog::initialize(&app);
    installShijimaEngineLogger();
    APP_LOG_INFO("startup") << "GUI startup initialized argc=" << argc
        << " cli_runtime=" << (startedForCli ? "1" : "0")
        << " startup_launch=" << (startedFromSystemStartup ? "1" : "0")
        << " version=\"" << NEUROLINGSCE_VERSION << "\"";
    std::set_terminate(appTerminateHandler);
#ifdef _WIN32
    SetUnhandledExceptionFilter(appUnhandledExceptionFilter);
#endif
    syncBundledSkillsForCurrentUser(&app);
    eApp->init();
    {
        // Prefer the icon resource that matches the OS shell expectation first,
        // then fall back to the PNG if the ICO resource is unavailable.
        QIcon appIcon { QStringLiteral(":/neurolingsce.ico") };
        if (appIcon.isNull()) {
            appIcon = QIcon { QStringLiteral(":/neurolingsce.png") };
        }
        if (!appIcon.isNull()) {
            app.setWindowIcon(appIcon);
        }
    }
    try {
        // If another instance is already alive, ask it to surface the manager
        // instead of starting a duplicate UI process.
        if (!startedForCli && startedFromSystemStartup && shijimaLocalApiPing()) {
            APP_LOG_INFO("startup") << "Existing instance detected during system startup; exiting silently";
            AppLog::shutdown();
            return 0;
        }
        if (!startedForCli && !startedFromSystemStartup && shijimaLocalApiShowManager()) {
            APP_LOG_INFO("startup") << "Existing instance activated; exiting current GUI launch request";
            AppLog::shutdown();
            return 0;
        }
        // The IPC ping is the last gate for the single-instance check.
        if (shijimaLocalApiPing()) {
            APP_LOG_ERROR("startup") << "Single-instance guard rejected startup; local IPC endpoint already responded";
            if (startedForCli) {
                AppLog::shutdown();
                return 0;
            }
            throw std::runtime_error(QCoreApplication::translate("main", APP_NAME " is already running!").toStdString());
        }
        APP_LOG_INFO("startup") << "Application startup checks passed";
        auto *manager = ShijimaManager::defaultManager();
        // The manager installs its own translators before creating visible
        // widgets.  Drop the short-lived bootstrap copies once construction
        // succeeds so translator ownership remains unambiguous.
        bootstrapTranslators.uninstall();
        if (!startedForCli && !startedFromSystemStartup) {
            manager->show();
        }
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("startup") << "Failed to start application: " << ex.what();
        if (startedForCli) {
            AppLog::shutdown();
            return 1;
        }
        QMessageBox *msg = new QMessageBox {};
        msg->setText(QCoreApplication::translate("main", APP_NAME " failed to start. Reason: ") +
            QString::fromUtf8(ex.what()));
        msg->setStandardButtons(QMessageBox::StandardButton::Close);
        msg->setAttribute(Qt::WA_DeleteOnClose);
        msg->show();
    }
    int ret = app.exec();
    APP_LOG_INFO("shutdown") << "Application event loop exited code=" << ret;
    ShijimaManager::finalize();
    AssetLoader::finalize();
    AppLog::shutdown();
    return ret;
}
