#pragma once

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

#include <QString>
#include "shijima-qt/CodexActivity.hpp"
#include "shijima-qt/CodexAppServerModels.hpp"
#include "shijima-qt/MascotStoreIndex.hpp"
#include <QList>
#include <QMap>
#include <QSet>
#include "shijima-qt/PlatformWidget.hpp"
#include <map>
#include <memory>
#include <set>
#include <list>
#include <functional>
#include <optional>
#include "ElaWindow.h"

class MascotData;
class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QLabel;
class QListWidgetItem;
class QObject;
class QPoint;
class QPushButton;
class QJsonObject;
class QScreen;
class QShowEvent;
class QSettings;
class QTimerEvent;
class QTranslator;
class QWidget;
class ShijimaWidget;
class GitHubUpdateManager;
class GitHubAuthManager;
class MascotStoreCache;
class MascotStoreCoordinator;
class MascotStoreNetwork;
class MascotSubmissionClient;
class ShijimaHttpApi;
class ShijimaLocalApi;
class CodexAppServerClient;
struct ShijimaManagerRuntimeState;
struct ShijimaManagerUiState;

class ShijimaManager : public PlatformWidget<ElaWindow>
{
    Q_OBJECT
public:
    static ShijimaManager *defaultManager();
    static void finalize();
    void updateEnvironment();
    void updateEnvironment(QScreen *);
    QString const& mascotsPath();
    ShijimaWidget *spawn(std::string const& name);
    bool showCodexNotification(CodexActivity const& activity);
    bool showCodexAppServerBubble(QString const& title, QString const& message,
        QString const& dedupeKey = {});
    // Navigate to the in-process app-server page.  This is intentionally a
    // UI-only action; it never starts a Codex process by itself.
    void showCodexPage();
    void killAll();
    void killAll(QString const& name);
    void killAllButOne(ShijimaWidget *widget);
    void killAllButOne(QString const& name);
    void setManagerVisible(bool visible);
    void importOnShow(QString const& path);
    void quitAction();
    std::set<std::string> import(QString const& path) noexcept;
    void reloadMascots(std::set<std::string> const& mascots);
    bool removeMascotTemplate(QString const& name, QString &errorMessage);
    // Compatibility facades for legacy UI/API callers. New manager internals
    // should prefer the runtime stores instead of spreading raw containers.
    QMap<QString, MascotData *> const& loadedMascots();
    QMap<int, MascotData *> const& loadedMascotsById();
    std::list<ShijimaWidget *> const& mascots();
    std::map<int, ShijimaWidget *> const& mascotsById();
    std::optional<int> cliLabelForMascot(int mascotId) const;
    std::optional<int> mascotIdForCliLabel(int cliLabel) const;
    bool assignCliLabel(int mascotId, std::optional<int> preferredLabel,
        int &assignedLabel, QString &errorMessage);
    void clearCliLabelForMascot(int mascotId);
    void clearCliLabels();
    ShijimaWidget *hitTest(QPoint const& screenPos);
    void onTickSync(std::function<void(ShijimaManager *)> callback);
    ~ShijimaManager();
protected:
    void timerEvent(QTimerEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    explicit ShijimaManager(QWidget *parent = nullptr);
    void abortPendingCallbacks();
    void shutdownForQuit();
    void closeManagerWindow();
    void loadDefaultMascot();
    void loadData(MascotData *data);
    void spawnClicked();
    void reloadMascot(QString const& name);
    void askClose();
    void itemDoubleClicked(QListWidgetItem *qItem);
    void loadAllMascots();
    void syncMascotLibrary();
    void refreshListWidget();
    void updateSelectedMascotDetails();
    void setupNavigation();
    void setupHomePage();
    void setupCreatePage();
    void setupCombinationsPage();
    void setupSettingsPage();
    void setupCodexPage();
    void setupAboutPage();
    void setupStorePage();
    void showAboutDialog();
    void showMascotStoreDetail(MascotStoreEntry const* entry);
    void importAction();
    void deleteAction();
    void updateSandboxBackground();
    bool windowedMode();
    QWidget *mascotParent();
    void setWindowedMode(bool windowedMode);
    void screenAdded(QScreen *);
    void screenRemoved(QScreen *);
    void importWithDialog(QList<QString> const& paths);
    void tick();
    bool prepareMascotTick();
    void tickMascotWidgets();
    void finishMascotTick();
    void refreshCombinationPage();
    void saveCurrentCombination();
    void restoreSelectedCombination();
    void deleteSelectedCombination();
    int restoreCombination(QJsonObject const& combination, bool showMessages);
    void restoreStartupCombination();
    void saveLastCombinationBeforeShutdown();
    void retranslateUi();
    void switchLanguage(const QString &langCode);
    void updateStatusBar();
    void startStartupUpdateCheck();
    void showStartupUpdateNotification(QString const& version);
    QScreen *mascotScreen();
    void updateCodexPageState();
    void updateCodexPagePlan(CodexPlanSnapshot const& snapshot);
    void updateCodexPageApprovals();
    void updateCodexPageInputs();
    void showCodexDiagnostic(QString const& message);
    std::unique_ptr<ShijimaManagerRuntimeState> m_runtime;
    std::unique_ptr<ShijimaManagerUiState> m_ui;
    std::unique_ptr<QSettings> m_settings;
    GitHubUpdateManager *m_updateManager = nullptr;
    std::unique_ptr<MascotStoreCache> m_storeCache;
    std::unique_ptr<MascotStoreNetwork> m_storeNetwork;
    std::unique_ptr<MascotStoreCoordinator> m_storeCoordinator;
    std::unique_ptr<GitHubAuthManager> m_githubAuth;
    std::unique_ptr<MascotSubmissionClient> m_submissionClient;
    MascotStoreIndex m_lastStoreIndex;
    bool m_allowClose = false;
    bool m_firstShow = true;
    bool m_wasVisible = false;
    bool m_constructing = true;
    std::unique_ptr<ShijimaHttpApi> m_httpApi;
    std::unique_ptr<ShijimaLocalApi> m_localApi;
};
