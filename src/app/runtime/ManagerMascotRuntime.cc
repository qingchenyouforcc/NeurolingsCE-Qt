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
#include "shijima-qt/MascotData.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/SafePath.hpp"
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"
#include "ManagerRuntimeState.hpp"
#include "../ui/ManagerUiState.hpp"
#include "../ui/ManagerUiHelpers.hpp"
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QListWidget>
#include <QRandomGenerator>
#include <QScreen>

using namespace shijima;

namespace {

void refreshMascotCounts(MascotSessionStore& sessions) {
    long count = sessions.size();
    for (auto mascot : sessions.mascots()) {
        if (mascot == nullptr || mascot->env() == nullptr) {
            continue;
        }
        mascot->env()->mascot_count = count;
    }
}

}

void ShijimaManager::killAll() {
    APP_LOG_INFO("mascot") << "Marking all mascots for deletion count="
        << m_runtime->sessions.size();
    m_runtime->sessions.markAllForDeletion();
}

void ShijimaManager::killAll(QString const& name) {
    APP_LOG_INFO("mascot") << "Marking mascots for deletion name=\""
        << name.toStdString() << "\"";
    m_runtime->sessions.markAllForDeletion(name);
}

void ShijimaManager::killAllButOne(ShijimaWidget *widget) {
    APP_LOG_INFO("mascot") << "Marking all mascots except one for deletion keep_id="
        << (widget != nullptr ? widget->mascotId() : -1);
    m_runtime->sessions.markAllButOneForDeletion(widget);
}

void ShijimaManager::killAllButOne(QString const& name) {
    APP_LOG_INFO("mascot") << "Marking all mascots except one for deletion name=\""
        << name.toStdString() << "\"";
    m_runtime->sessions.markAllButOneForDeletion(name);
}

void ShijimaManager::loadData(MascotData *data) {
    auto *loaded = m_runtime->templates.registerTemplate(
        std::unique_ptr<MascotData>(data));
    APP_LOG_INFO("mascot") << "Loaded mascot template name=\""
        << loaded->name().toStdString() << "\" id=" << loaded->id()
        << " path=\"" << loaded->path().toStdString() << "\"";
}

void ShijimaManager::loadDefaultMascot() {
    APP_LOG_INFO("mascot") << "Loading default mascot template";
    auto data = new MascotData { "@", m_runtime->idCounter++ };
    loadData(data);
}

QMap<QString, MascotData *> const& ShijimaManager::loadedMascots() {
    return m_runtime->templates.loadedMascots();
}

QMap<int, MascotData *> const& ShijimaManager::loadedMascotsById() {
    return m_runtime->templates.loadedMascotsById();
}

std::list<ShijimaWidget *> const& ShijimaManager::mascots() {
    return m_runtime->sessions.mascots();
}

std::map<int, ShijimaWidget *> const& ShijimaManager::mascotsById() {
    return m_runtime->sessions.mascotsById();
}

std::optional<int> ShijimaManager::cliLabelForMascot(int mascotId) const {
    return m_runtime->sessions.cliLabelForMascot(mascotId);
}

std::optional<int> ShijimaManager::mascotIdForCliLabel(int cliLabel) const {
    return m_runtime->sessions.mascotIdForCliLabel(cliLabel);
}

bool ShijimaManager::assignCliLabel(int mascotId,
    std::optional<int> preferredLabel, int &assignedLabel, QString &errorMessage)
{
    return m_runtime->sessions.assignCliLabel(mascotId, preferredLabel,
        assignedLabel, errorMessage);
}

void ShijimaManager::clearCliLabelForMascot(int mascotId) {
    m_runtime->sessions.clearCliLabelForMascot(mascotId);
}

void ShijimaManager::clearCliLabels() {
    m_runtime->sessions.clearCliLabels();
}

void ShijimaManager::reloadMascot(QString const& name) {
    auto *loadedData = m_runtime->templates.loadedMascots().value(name, nullptr);
    if (loadedData != nullptr && !loadedData->deletable()) {
        APP_LOG_WARN("mascot") << "Refusing to unload non-deletable mascot template name=\""
            << name.toStdString() << "\"";
        return;
    }

    MascotData *newData = nullptr;
    try {
        newData = new MascotData {
            MascotPackage::packagePathForName(m_runtime->mascotsPath, name),
            MascotPackage::cachePathForName(m_runtime->mascotCachePath, name),
            m_runtime->idCounter++
        };
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Failed to reload mascot template name=\""
            << name.toStdString() << "\": " << ex.what();
    }

    if (m_runtime->templates.loadedMascots().contains(name)) {
        auto oldData = m_runtime->templates.takeTemplate(name);
        if (oldData != nullptr) {
            oldData->unloadCache();
        }
        killAll(name);
        APP_LOG_INFO("mascot") << "Unloaded mascot template name=\""
            << name.toStdString() << "\"";
    }

    if (newData != nullptr) {
        if (newData->name() != name) {
            throw std::runtime_error("Impossible condition: New mascot name is incorrect");
        }
        loadData(newData);
    }

    m_runtime->listItemsToRefresh.insert(name);
    ShijimaManagerUiInternal::refreshTrayMenu(m_ui->trayController.get());
}

void ShijimaManager::refreshListWidget() {
    QSet<QString> selectedNames;
    for (auto *item : m_ui->listWidget->selectedItems()) {
        selectedNames.insert(item->text());
    }

    m_ui->listWidget->clear();
    auto names = m_runtime->templates.loadedMascots().keys();
    names.sort(Qt::CaseInsensitive);
    for (auto &name : names) {
        auto item = new QListWidgetItem;
        item->setText(name);
        auto *data = m_runtime->templates.loadedMascots()[name];
        item->setIcon(data->preview());
        item->setSizeHint(QSize(0, 72));
        QStringList tooltip;
        tooltip.append(data->name());
        if (!data->metadata().version.isEmpty()) {
            tooltip.append(tr("Version: %1").arg(data->metadata().version));
        }
        if (!data->metadata().author.isEmpty()) {
            tooltip.append(tr("Author: %1").arg(data->metadata().author));
        }
        item->setToolTip(tooltip.join(QLatin1Char('\n')));
        if (selectedNames.contains(name)) {
            item->setSelected(true);
        }
        m_ui->listWidget->addItem(item);
    }
    m_runtime->listItemsToRefresh.clear();
    updateSelectedMascotDetails();
    updateStatusBar();
}

void ShijimaManager::loadAllMascots() {
    APP_LOG_INFO("mascot") << "Loading mascot library path=\""
        << m_runtime->mascotsPath.toStdString() << "\"";
    int loadedCount = 0;
    int skippedCount = 0;
    QDirIterator iter { m_runtime->mascotsPath, QStringList { QStringLiteral("*.mascot") },
        QDir::Files,
        QDirIterator::NoIteratorFlags };
    while (iter.hasNext()) {
        auto packagePath = iter.nextFileInfo().absoluteFilePath();
        MascotMetadata metadata;
        QString errorMessage;
        if (!MascotPackage::inspectPackage(packagePath, metadata, errorMessage)) {
            APP_LOG_WARN("mascot") << "Skipping invalid mascot package path=\""
                << packagePath.toStdString() << "\": "
                << errorMessage.toStdString();
            ++skippedCount;
            continue;
        }
        QString canonicalPath = QFileInfo(packagePath).absoluteFilePath();
        QString expectedPath = QFileInfo(MascotPackage::packagePathForName(
            m_runtime->mascotsPath, metadata.name)).absoluteFilePath();
        if (canonicalPath != expectedPath) {
            if (QFile::exists(expectedPath) && !QFile::remove(expectedPath)) {
                APP_LOG_WARN("mascot") << "Failed to remove existing mascot package path=\""
                    << expectedPath.toStdString() << "\"";
                ++skippedCount;
                continue;
            }
            if (!QFile::rename(canonicalPath, expectedPath)) {
                APP_LOG_WARN("mascot") << "Failed to normalize mascot package path from=\""
                    << canonicalPath.toStdString() << "\" to=\""
                    << expectedPath.toStdString() << "\"";
                ++skippedCount;
                continue;
            }
        }
        reloadMascot(metadata.name);
        ++loadedCount;
    }
    refreshListWidget();
    APP_LOG_INFO("mascot") << "Mascot library loaded packages=" << loadedCount
        << " skipped=" << skippedCount;
}

void ShijimaManager::syncMascotLibrary() {
    APP_LOG_INFO("mascot") << "Syncing mascot library path=\""
        << m_runtime->mascotsPath.toStdString() << "\"";
    QSet<QString> mascotsOnDisk;
    QDirIterator iter { m_runtime->mascotsPath, QStringList { QStringLiteral("*.mascot") },
        QDir::Files,
        QDirIterator::NoIteratorFlags };
    while (iter.hasNext()) {
        auto packagePath = iter.nextFileInfo().absoluteFilePath();
        MascotMetadata metadata;
        QString errorMessage;
        if (!MascotPackage::inspectPackage(packagePath, metadata, errorMessage)) {
            continue;
        }
        QString canonicalPath = QFileInfo(packagePath).absoluteFilePath();
        QString expectedPath = QFileInfo(MascotPackage::packagePathForName(
            m_runtime->mascotsPath, metadata.name)).absoluteFilePath();
        if (canonicalPath != expectedPath) {
            bool removed = QFile::remove(expectedPath);
            bool renamed = QFile::rename(canonicalPath, expectedPath);
            APP_LOG_INFO("mascot") << "Normalized mascot package path from=\""
                << canonicalPath.toStdString() << "\" to=\""
                << expectedPath.toStdString() << "\" removed_existing="
                << (removed ? "1" : "0") << " renamed=" << (renamed ? "1" : "0");
        }
        mascotsOnDisk.insert(metadata.name);
    }

    auto loadedNames = m_runtime->templates.loadedMascots().keys();
    for (auto const& name : loadedNames) {
        auto *data = m_runtime->templates.loadedMascots().value(name, nullptr);
        if (data == nullptr || !data->deletable()) {
            continue;
        }
        if (!mascotsOnDisk.contains(name)) {
            reloadMascot(name);
        }
    }

    for (auto const& name : mascotsOnDisk) {
        reloadMascot(name);
    }

    refreshListWidget();
    APP_LOG_INFO("mascot") << "Mascot library sync completed disk_templates="
        << mascotsOnDisk.size();
}

void ShijimaManager::reloadMascots(std::set<std::string> const& mascots) {
    for (auto &mascot : mascots) {
        reloadMascot(QString::fromStdString(mascot));
    }
    refreshListWidget();
}

bool ShijimaManager::removeMascotTemplate(QString const& name,
    QString &errorMessage)
{
    if (!m_runtime->templates.loadedMascots().contains(name)) {
        errorMessage = QStringLiteral("No such mascot template");
        return false;
    }

    auto *mascotData = m_runtime->templates.loadedMascots()[name];
    if (mascotData == nullptr) {
        errorMessage = QStringLiteral("No such mascot template");
        return false;
    }
    if (!mascotData->deletable()) {
        errorMessage = QStringLiteral("Mascot template cannot be deleted");
        return false;
    }

    auto safePath = SafePath::safeChildPath(m_runtime->mascotsPath,
        QFileInfo(mascotData->packagePath()).fileName());
    if (!safePath.has_value() || QFileInfo(safePath.value()).absoluteFilePath() !=
        QFileInfo(mascotData->packagePath()).absoluteFilePath())
    {
        errorMessage = QStringLiteral("Mascot package path is outside storage");
        return false;
    }
    std::filesystem::path path = safePath->toStdString();
    APP_LOG_INFO("mascot") << "Deleting mascot template name=\""
        << name.toStdString() << "\" path=\"" << path.string() << "\"";
    try {
        std::filesystem::remove(path);
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Failed to delete mascot template path=\""
            << path.string() << "\": " << ex.what();
        errorMessage = QString::fromUtf8(ex.what());
        return false;
    }

    reloadMascot(name);
    refreshListWidget();
    return true;
}

bool ShijimaManager::prepareMascotTick() {
    if (m_ui->sandboxWidget != nullptr && !m_ui->sandboxWidget->isVisible()) {
        setWindowedMode(false);
#if !defined(__APPLE__)
        if (m_runtime->sessions.empty() && !m_runtime->cliRuntimeMode &&
            !m_runtime->silentStartupMode)
        {
            setManagerVisible(true);
        }
#endif
    }

    if (m_runtime->sessions.empty()) {
#if !defined(__APPLE__)
        if (!windowedMode() && !m_wasVisible && !m_runtime->cliRuntimeMode &&
            !m_runtime->silentStartupMode)
        {
            setManagerVisible(true);
        }
#endif
        return false;
    }

    return true;
}

void ShijimaManager::tickMascotWidgets() {
    auto &sessions = m_runtime->sessions;
    refreshMascotCounts(sessions);
    for (auto iter = sessions.mascots().end(); iter != sessions.mascots().begin(); ) {
        --iter;
        ShijimaWidget *shimeji = *iter;
        if (!shimeji->isVisible()) {
            int mascotId = shimeji->mascotId();
            APP_LOG_INFO("mascot") << "Removing invisible mascot widget id="
                << mascotId;
            delete shimeji;
            auto erasePos = iter;
            ++iter;
            sessions.mascots().erase(erasePos);
            sessions.removeIndex(mascotId);
            refreshMascotCounts(sessions);
            continue;
        }

        auto envOverride = shimeji->prepareTickEnvironmentOverride();
        double anchorYBefore = shimeji->mascot().state->anchor.y;
        shimeji->tick();
        shimeji->restoreTickEnvironmentOverride(envOverride);

        if (!windowedMode()) {
            double anchorYAfter = shimeji->mascot().state->anchor.y;
            shimeji->resetFallThroughTrackingIfDragged();
            shimeji->observeFallProgress(anchorYBefore, anchorYAfter);
        }

        auto &mascot = shimeji->mascot();
        auto &breedRequest = mascot.state->breed_request;
        if (mascot.state->dragging && !windowedMode()) {
            auto oldScreen = m_runtime->environment.screenForEnvironment(
                mascot.state->env.get());
            auto newScreen = QGuiApplication::screenAt(QPoint {
                (int)mascot.state->anchor.x, (int)mascot.state->anchor.y });
            if (newScreen != nullptr && oldScreen != newScreen) {
                mascot.state->env = m_runtime->environment.environmentForScreen(newScreen);
            }
        }
        if (breedRequest.available) {
            if (breedRequest.name == "") {
                breedRequest.name = shimeji->mascotName().toStdString();
            }
            breedRequest.name = breedRequest.name.substr(breedRequest.name.rfind('\\') + 1);
            breedRequest.name = breedRequest.name.substr(breedRequest.name.rfind('/') + 1);

            std::optional<shijima::mascot::factory::product> product;
            APP_LOG_INFO("mascot") << "Breed request received parent_id="
                << shimeji->mascotId() << " child_name=\"" << breedRequest.name << "\"";
            try {
                product = m_runtime->templates.factory().spawn(breedRequest);
            }
            catch (std::exception &ex) {
                APP_LOG_ERROR("mascot") << "Failed to fulfill breed request name=\""
                    << breedRequest.name << "\": " << ex.what();
            }

            if (product.has_value()) {
                ShijimaWidget *child = new ShijimaWidget(
                    m_runtime->templates.loadedMascots()[QString::fromStdString(breedRequest.name)],
                    std::move(product->manager), m_runtime->idCounter++,
                    windowedMode(), mascotParent());
                child->setEnv(shimeji->env());
                if (!child->primeInitialFrame()) {
                    APP_LOG_WARN("mascot") << "Breed child failed initial frame name=\""
                        << breedRequest.name << "\"";
                    delete child;
                    breedRequest.available = false;
                    continue;
                }
                child->show();
                m_runtime->sessions.add(child);
                refreshMascotCounts(sessions);
                APP_LOG_INFO("mascot") << "Breed request succeeded child_id="
                    << child->mascotId() << " child_name=\"" << breedRequest.name << "\"";
            }
            breedRequest.available = false;
        }
    }
}

void ShijimaManager::finishMascotTick() {
    m_runtime->environment.resetScales();

    if (m_runtime->sessions.empty() && !windowedMode() &&
        !m_runtime->cliRuntimeMode && !m_runtime->silentStartupMode)
    {
        setManagerVisible(true);
    }

    updateStatusBar();
}

void ShijimaManager::tick() {
    if (!prepareMascotTick()) {
        return;
    }

    updateEnvironment();
    tickMascotWidgets();
    finishMascotTick();
}

ShijimaWidget *ShijimaManager::hitTest(QPoint const& screenPos) {
    for (auto mascot : m_runtime->sessions.mascots()) {
        QPoint localPos = { screenPos.x() - mascot->x(),
            screenPos.y() - mascot->y() };
        if (mascot->pointInside(localPos)) {
            return mascot;
        }
    }
    return nullptr;
}

ShijimaWidget *ShijimaManager::spawn(std::string const& name) {
    APP_LOG_INFO("mascot") << "Spawn requested for template name=\"" << name << "\"";

    try {
        auto templateName = QString::fromStdString(name);
        auto *mascotData = m_runtime->templates.loadedMascots().value(
            templateName, nullptr);
        if (mascotData == nullptr) {
            APP_LOG_ERROR("mascot") << "Spawn rejected for unknown template name=\""
                << name << "\"";
            return nullptr;
        }

        QScreen *screen = mascotScreen();
        if (screen == nullptr && !windowedMode()) {
            screen = QGuiApplication::primaryScreen();
        }
        updateEnvironment(screen);
        auto env = m_runtime->environment.environmentForScreen(screen);
        if (env == nullptr) {
            APP_LOG_ERROR("mascot") << "Spawn failed because no environment is available"
                << " for template name=\"" << name << "\"";
            return nullptr;
        }
        auto product = m_runtime->templates.factory().spawn(name, {});
        product.manager->state->env = env;
        product.manager->reset_position();
        ShijimaWidget *shimeji = new ShijimaWidget(
            mascotData,
            std::move(product.manager), m_runtime->idCounter++,
            windowedMode(), mascotParent());
        if (!shimeji->primeInitialFrame()) {
            delete shimeji;
            return nullptr;
        }
        shimeji->show();
        m_runtime->sessions.add(shimeji);
        env->reset_scale();
        APP_LOG_INFO("mascot") << "Spawn succeeded for template name=\"" << name
            << "\" mascotId=" << shimeji->mascotId();
        return shimeji;
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Spawn failed for template name=\"" << name
            << "\": " << ex.what();
    }
    catch (...) {
        APP_LOG_ERROR("mascot") << "Spawn failed for template name=\"" << name
            << "\" with unknown exception";
    }
    return nullptr;
}

void ShijimaManager::spawnClicked() {
    auto const& loadedMascots = m_runtime->templates.loadedMascots();
    if (loadedMascots.isEmpty()) {
        APP_LOG_WARN("mascot") << "Random spawn ignored because no templates are loaded";
        return;
    }

    int target = QRandomGenerator::global()->bounded(loadedMascots.size());
    auto it = loadedMascots.cbegin();
    std::advance(it, target);
    std::string name = it.key().toStdString();
    APP_LOG_INFO("mascot") << "Spawning random mascot template name=\"" << name << "\"";
    spawn(name);
}
