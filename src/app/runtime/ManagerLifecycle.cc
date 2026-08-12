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
#include "shijima-qt/GitHubAuthManager.hpp"
#include "shijima-qt/MascotStoreCache.hpp"
#include "shijima-qt/MascotStoreCoordinator.hpp"
#include "shijima-qt/MascotStoreNetwork.hpp"
#include "shijima-qt/MascotSubmissionClient.hpp"
#include "shijima-qt/ShijimaHttpApi.hpp"
#include "shijima-qt/ShijimaLocalApi.hpp"
#include "shijima-qt/CodexAppServerClient.hpp"

#include "ManagerRuntimeState.hpp"
#include "../ui/ManagerUiState.hpp"
#include "ManagerRuntimeHelpers.hpp"
#include "../ui/ManagerUiHelpers.hpp"
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QSettings>
#include <QThread>

static ShijimaManager *m_defaultManager = nullptr;

ShijimaManager *ShijimaManager::defaultManager() {
    if (m_defaultManager == nullptr) {
        // The application assumes a single manager instance and uses this
        // singleton-style entry point to make that explicit.
        APP_LOG_INFO("lifecycle") << "Creating default ShijimaManager";
        m_defaultManager = new ShijimaManager;
    }
    return m_defaultManager;
}

void ShijimaManager::finalize() {
    if (m_defaultManager != nullptr) {
        // Tear down through the same global pointer so shutdown order stays
        // predictable when CLI and GUI share the process.
        APP_LOG_INFO("lifecycle") << "Finalizing default ShijimaManager";
        delete m_defaultManager;
        m_defaultManager = nullptr;
    }
}

QString const& ShijimaManager::mascotsPath() {
    return m_runtime->mascotsPath;
}

ShijimaManager::~ShijimaManager() {
    APP_LOG_INFO("lifecycle") << "Destroying ShijimaManager";
    if (m_runtime != nullptr && m_runtime->codexClient != nullptr) {
        m_runtime->codexClient->cancelPendingRequests();
        m_runtime->codexClient->stop();
    }
    disconnect(qApp, &QGuiApplication::screenAdded,
        this, &ShijimaManager::screenAdded);
    disconnect(qApp, &QGuiApplication::screenRemoved,
        this, &ShijimaManager::screenRemoved);
}

void ShijimaManager::abortPendingCallbacks() {
    APP_LOG_DEBUG("lifecycle") << "Aborting pending manager callbacks";
    m_runtime->shuttingDown.store(true);
}

void ShijimaManager::shutdownForQuit() {
    APP_LOG_INFO("shutdown") << "Manager shutdown started mascot_count="
        << m_runtime->sessions.size();
    saveLastCombinationBeforeShutdown();
    // Stop new callbacks before destroying widgets or timers.
    abortPendingCallbacks();
    ShijimaManagerUiInternal::teardownTrayIcon(m_ui->trayController);

    if (m_runtime->mascotTimer > 0) {
        // Timer ownership is manual because Qt timer ids are not QObject-based.
        APP_LOG_DEBUG("shutdown") << "Stopping mascot timer id="
            << m_runtime->mascotTimer;
        killTimer(m_runtime->mascotTimer);
        m_runtime->mascotTimer = 0;
    }
    if (m_runtime->windowObserverTimer > 0) {
        APP_LOG_DEBUG("shutdown") << "Stopping window observer timer id="
            << m_runtime->windowObserverTimer;
        killTimer(m_runtime->windowObserverTimer);
        m_runtime->windowObserverTimer = 0;
    }

    m_localApi->stop();
    m_httpApi->stop();
    // Stop the explicitly managed app-server before destroying mascot/UI
    // state. Pending approvals are cancelled best-effort by the client and
    // no process restart is attempted during shutdown.
    if (m_runtime->codexClient != nullptr) {
        m_runtime->codexClient->cancelPendingRequests();
        m_runtime->codexClient->stop();
    }

    // Close mascots manually because they are top-level widgets, not child
    // objects owned by the manager window.
    for (auto mascot : m_runtime->sessions.mascots()) {
        if (mascot == nullptr) {
            continue;
        }
        mascot->markForDeletion();
        mascot->hide();
        mascot->close();
        delete mascot;
    }
    m_runtime->sessions.clear();

    if (m_ui->sandboxWidget != nullptr) {
        m_ui->sandboxWidget->close();
        delete m_ui->sandboxWidget;
        m_ui->sandboxWidget = nullptr;
    }
    APP_LOG_INFO("shutdown") << "Manager shutdown completed";
}

void ShijimaManager::onTickSync(std::function<void(ShijimaManager *)> callback) {
    if (m_runtime->shuttingDown.load()) {
        return;
    }

    if (QThread::currentThread() == thread()) {
        callback(this);
        return;
    }

    // HTTP and Local IPC callbacks run on worker threads; mascot state belongs
    // to the Qt GUI thread, so mutations are marshalled here synchronously.
    QMetaObject::invokeMethod(this, [this, callback]() {
        if (!m_runtime->shuttingDown.load()) {
            callback(this);
        }
    }, Qt::BlockingQueuedConnection);
}

void ShijimaManager::closeEvent(QCloseEvent *event) {
    APP_LOG_INFO("lifecycle") << "Manager close event allow_close="
        << (m_allowClose ? "1" : "0");
#if !defined(__APPLE__)
    if (!m_allowClose) {
        event->ignore();
#if defined(_WIN32)
        if (m_runtime->sessions.empty()) {
            askClose();
        }
        else {
            setManagerVisible(false);
        }
#else
        askClose();
#endif
        return;
    }

    shutdownForQuit();
    event->accept();
    QMetaObject::invokeMethod(qApp, []() {
        QCoreApplication::quit();
    }, Qt::QueuedConnection);
#else
    event->ignore();
    setManagerVisible(false);
#endif
}

void ShijimaManager::timerEvent(QTimerEvent *event) {
    int timerId = event->timerId();
    if (timerId == m_runtime->mascotTimer) {
        tick();
    }
    else if (timerId == m_runtime->windowObserverTimer) {
        m_runtime->environment.windowObserver().tick();
    }
}
