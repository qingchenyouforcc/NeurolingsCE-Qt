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

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/ShijimaHttpApi.hpp"
#include "shijima-qt/ShijimaLocalApi.hpp"
#include "../core/update/GitHubUpdateManager.hpp"

#include "../runtime/ManagerRuntimeState.hpp"
#include "ManagerUiState.hpp"
#include "../runtime/ManagerRuntimeHelpers.hpp"
#include "ManagerUiHelpers.hpp"

#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QVariant>

#include "ElaStatusBar.h"
#include "ElaTheme.h"

ShijimaManager::ShijimaManager(QWidget *parent):
    PlatformWidget(parent),
    m_runtime(std::make_unique<ShijimaManagerRuntimeState>()),
    m_ui(std::make_unique<ShijimaManagerUiState>()),
    m_settings(std::make_unique<QSettings>("pixelomer", "Shijima-Qt")),
    m_httpApi(std::make_unique<ShijimaHttpApi>(this)),
    m_localApi(std::make_unique<ShijimaLocalApi>(this))
{
    m_runtime->cliRuntimeMode =
        qApp->property("neurolingsce.cliRuntime").toBool();
    m_runtime->silentStartupMode =
        !m_runtime->cliRuntimeMode &&
        qApp->property("neurolingsce.startupLaunch").toBool() &&
        m_settings->value("startup/silent", false).toBool();
    m_ui->listWidget = new QListWidget(this);

    for (auto screen : QGuiApplication::screens()) {
        screenAdded(screen);
    }
    screenAdded(nullptr);

    connect(qApp, &QGuiApplication::screenAdded,
        this, &ShijimaManager::screenAdded);
    connect(qApp, &QGuiApplication::screenRemoved,
        this, &ShijimaManager::screenRemoved);

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString mascotsPath = QDir::cleanPath(dataPath + QDir::separator() + "mascots");
    QString mascotCachePath = QDir::cleanPath(dataPath + QDir::separator() + "mascot-cache");
    QDir mascotsDir(mascotsPath);
    if (!mascotsDir.exists()) {
        mascotsDir.mkpath(mascotsPath);
    }
    QDir cacheDir(mascotCachePath);
    if (!cacheDir.exists()) {
        cacheDir.mkpath(mascotCachePath);
    }
    if (QFile readme { mascotsDir.absoluteFilePath("README.txt") };
        readme.open(QFile::WriteOnly | QFile::NewOnly | QFile::Text))
    {
        readme.write(""
"Manually importing shimeji by copying its contents into this folder may\n"
"cause problems. You should use the import dialog in Shijima-Qt unless you\n"
"have a good reason not to.\n"
        );
        readme.close();
    }
    m_runtime->mascotsPath = mascotsPath;
    m_runtime->mascotCachePath = mascotCachePath;
    MascotPackage::migrateLegacyDirectories(m_runtime->mascotsPath);
    APP_LOG_INFO("startup") << "Mascot storage path=\""
        << m_runtime->mascotsPath.toStdString() << "\"";
    m_updateManager = new GitHubUpdateManager(m_settings.get(), this);
    connect(m_updateManager, &GitHubUpdateManager::startupUpdateAvailable,
        this, &ShijimaManager::showStartupUpdateNotification);

    loadDefaultMascot();
    loadAllMascots();
    setAcceptDrops(true);

    m_runtime->mascotTimer = startTimer(40 / ShijimaManagerRuntimeInternal::kSubtickCount);
    if (m_runtime->environment.windowObserver().tickFrequency() > 0) {
        m_runtime->windowObserverTimer = startTimer(
            m_runtime->environment.windowObserver().tickFrequency());
    }

    setUserInfoCardVisible(false);
    setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
    setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
    setIsDefaultClosed(false);
    setMinimumHeight(450);
    connect(this, &ElaWindow::closeButtonClicked, this, [this]() {
#if defined(_WIN32)
        if (m_runtime->sessions.empty()) {
            askClose();
        } else {
            setManagerVisible(false);
        }
#else
        askClose();
#endif
    });

    auto syncTheme = [](Qt::ColorScheme scheme) {
        eTheme->setThemeMode(scheme == Qt::ColorScheme::Dark
            ? ElaThemeType::Dark : ElaThemeType::Light);
    };
    syncTheme(QGuiApplication::styleHints()->colorScheme());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
        this, syncTheme);

    connect(m_ui->listWidget, &QListWidget::itemDoubleClicked,
        this, &ShijimaManager::itemDoubleClicked);
    connect(m_ui->listWidget, &QListWidget::itemSelectionChanged,
        this, &ShijimaManager::updateSelectedMascotDetails);
    m_ui->listWidget->setIconSize({ 64, 64 });
    m_ui->listWidget->installEventFilter(this);
    m_ui->listWidget->setAcceptDrops(true);
    m_ui->listWidget->setSelectionMode(QListWidget::ExtendedSelection);
    ShijimaManagerUiInternal::applyMascotListTheme(*m_ui->listWidget);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() {
        ShijimaManagerUiInternal::applyMascotListTheme(*m_ui->listWidget);
    });

    setWindowTitle(tr(APP_NAME " \u2014 Mascot Manager"));
    auto *elaStatusBar = new ElaStatusBar(this);
    setStatusBar(elaStatusBar);
    m_ui->statusLabel = new QLabel(this);
    elaStatusBar->addWidget(m_ui->statusLabel, 1);
    updateStatusBar();

    QString savedLang = m_settings->value("language", "en").toString();
    if (savedLang != "en") {
        m_ui->currentLanguage = "en";
        switchLanguage(savedLang);
    }

    m_runtime->environment.setDetachThreshold(m_settings->value("detachThreshold",
        QVariant::fromValue(30.0)).toDouble());
    m_runtime->environment.setUserScale(m_settings->value("userScale",
        QVariant::fromValue(1.0)).toDouble());

    if (!m_runtime->cliRuntimeMode) {
        setupNavigation();
    }
    if (!m_runtime->cliRuntimeMode && !m_runtime->silentStartupMode) {
        setManagerVisible(true);
    }
    m_constructing = false;

    if (!m_runtime->cliRuntimeMode) {
        ShijimaManagerUiInternal::setupTrayIcon(this, m_ui->trayController);
    }
    if (m_runtime->silentStartupMode) {
        QTimer::singleShot(0, this, [this]() {
            restoreStartupCombination();
        });
    }
    m_localApi->start();
    if (!m_runtime->cliRuntimeMode &&
        m_settings->value("http/enabled", false).toBool())
    {
        if (!m_httpApi->start("127.0.0.1", 32456)) {
            QString error = QString::fromStdString(m_httpApi->lastError());
            if (error.isEmpty()) {
                error = tr("HTTP API bind failed. The port may already be in use.");
            }
            APP_LOG_ERROR("http") << error.toStdString();
            QTimer::singleShot(0, this, [this, error]() {
                QMessageBox::warning(
                    this,
                    tr("HTTP API Failed"),
                    error);
            });
        }
    }
    if (!m_runtime->cliRuntimeMode) {
        startStartupUpdateCheck();
    }
    APP_LOG_INFO("startup") << "Manager window initialized";
}

bool ShijimaManager::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_ui->listWidget) {
        if (event->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent *>(event));
            return event->isAccepted();
        }
        if (event->type() == QEvent::DragMove) {
            auto *dragMoveEvent = static_cast<QDragMoveEvent *>(event);
            dragMoveEvent->acceptProposedAction();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent *>(event));
            return event->isAccepted();
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        auto key = keyEvent->key();
        if (key == Qt::Key::Key_Return || key == Qt::Key::Key_Enter) {
            for (auto item : m_ui->listWidget->selectedItems()) {
                itemDoubleClicked(item);
            }
            return true;
        }
    }
    return ElaWindow::eventFilter(obj, event);
}

void ShijimaManager::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange && !m_constructing) {
        retranslateUi();
    }
    else if (event->type() == QEvent::WindowStateChange) {
        ShijimaManagerUiInternal::refreshTrayMenu(m_ui->trayController.get());
    }
    PlatformWidget::changeEvent(event);
}

void ShijimaManager::startStartupUpdateCheck()
{
    if (m_updateManager == nullptr) {
        return;
    }
    if (!m_settings->value("update/checkOnStartup", true).toBool()) {
        return;
    }

    QTimer::singleShot(1500, this, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->checkForUpdates(GitHubUpdateManager::CheckMode::Startup);
        }
    });
}

void ShijimaManager::showStartupUpdateNotification(QString const& version)
{
    ShijimaManagerUiInternal::showTrayMessage(
        tr("Update Available"),
        tr("NeurolingsCE %1 is available. Click to open the About page.")
            .arg(QStringLiteral("v%1").arg(version)),
        m_ui->trayController.get(),
        [this]() {
            setManagerVisible(true);
            showAboutDialog();
        });
}
