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

#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"

#include <QDir>
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/ui/menus/ShijimaContextMenu.hpp"
#include "shijima-qt/ui/widgets/SpeechBubbleWidget.hpp"
#include "shijima-qt/ui/dialogs/inspector/ShimejiInspectorDialog.hpp"

ShijimaWidget::ShijimaWidget(MascotData *mascotData,
    std::unique_ptr<shijima::mascot::manager> mascot,
    int mascotId, bool windowedMode, QWidget *parent):
#if defined(__APPLE__)
    PlatformWidget(nullptr, PlatformWidget::ShowOnAllDesktops),
#else
    PlatformWidget(parent, PlatformWidget::ShowOnAllDesktops),
#endif
    m_windowedMode(windowedMode), m_data(mascotData),
    m_inspector(nullptr), m_mascotId(mascotId)
{
    m_windowHeight = 128;
    m_windowWidth = 128;
    m_mascot = std::move(mascot);

    QDir dir { m_data->imgRoot() };
    if (dir.exists() && dir.cdUp() && dir.cd("sound")) {
        m_sounds.searchPaths.push_back(dir.path());
    }

    // Speech bubble click reset timer
    m_clickResetTimer.setSingleShot(true);
    connect(&m_clickResetTimer, &QTimer::timeout, [this]() {
        m_clickCount = 0;
    });

    if (!m_windowedMode) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        Qt::WindowFlags flags = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint
            | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint
            | Qt::WindowOverridesSystemGestures;
#if defined(__APPLE__)
        flags |= Qt::Window;
#else
        flags |= Qt::Tool;
#endif
        setWindowFlags(flags);
    }
    setFixedSize(m_windowWidth, m_windowHeight);
}

ShijimaWidget::ShijimaWidget(ShijimaWidget &old, bool windowedMode,
    QWidget *parent) : ShijimaWidget(old.mascotData(),
    std::move(old.m_mascot), old.m_mascotId,
    windowedMode, parent) {}

void ShijimaWidget::showInspector() {
    if (m_inspector == nullptr) {
        m_inspector = new ShimejiInspectorDialog { this };
    }
    m_inspector->show();
}

bool ShijimaWidget::inspectorVisible() {
    return m_inspector != nullptr && m_inspector->isVisible();
}

bool ShijimaWidget::primeInitialFrame() {
    try {
        if (m_mascot->state->active_frame
            .get_name(m_mascot->state->looking_right).empty())
        {
            m_mascot->tick();
        }
        updateOffsets();
        return true;
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Failed to initialize mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString() << "\": " << ex.what();
    }
    catch (...) {
        APP_LOG_ERROR("mascot") << "Failed to initialize mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString()
            << "\" with unknown exception";
    }
    markForDeletion();
    close();
    return false;
}

bool ShijimaWidget::dragging() const {
    return m_mascot->state->dragging;
}

void ShijimaWidget::setDragging(bool dragging) {
    m_mascot->state->dragging = dragging;
}

ShijimaWidget::TickEnvironmentOverride
ShijimaWidget::prepareTickEnvironmentOverride() {
    TickEnvironmentOverride overrideState;
    if (!m_fallThroughMode) {
        return overrideState;
    }

    // Long falls intentionally bypass the taskbar floor and land at the real
    // screen bottom; this preserves that behavior without leaking env details.
    auto mascotEnv = env();
    overrideState.active = true;
    overrideState.floorY = mascotEnv->floor.y;
    overrideState.workAreaBottom = mascotEnv->work_area.bottom;
    mascotEnv->floor.y = mascotEnv->screen.bottom;
    mascotEnv->work_area.bottom = mascotEnv->screen.bottom;
    return overrideState;
}

void ShijimaWidget::restoreTickEnvironmentOverride(
    TickEnvironmentOverride const& overrideState)
{
    if (!overrideState.active) {
        return;
    }
    auto mascotEnv = env();
    mascotEnv->floor.y = overrideState.floorY;
    mascotEnv->work_area.bottom = overrideState.workAreaBottom;
}

void ShijimaWidget::resetFallThroughTrackingIfDragged() {
    if (!dragging() || !m_fallThroughMode) {
        return;
    }
    m_fallThroughMode = false;
    m_fallTracking = false;
}

void ShijimaWidget::observeFallProgress(double anchorYBefore,
    double anchorYAfter)
{
    bool onLand = m_mascot->state->on_land();
    if (!onLand && !dragging() && anchorYAfter > anchorYBefore) {
        if (!m_fallTracking) {
            m_fallTracking = true;
            m_fallStartY = anchorYBefore;
        }
        double fallDistance = anchorYAfter - m_fallStartY;
        if (fallDistance >= 700.0) {
            m_fallThroughMode = true;
        }
    }
    else {
        m_fallTracking = false;
    }
}

void ShijimaWidget::tick() {
    try {
        if (m_markedForDeletion) {
            close();
            return;
        }
        if (paused()) {
            return;
        }

        maintainHotspotHold();

        // Tick
        auto prev_frame = m_mascot->state->active_frame;
        m_mascot->tick();
        auto &new_frame = m_mascot->state->active_frame;
        auto &new_sound = m_mascot->state->active_sound;
        bool forceRepaint = prev_frame.name != new_frame.name;
        bool offsetsChanged = updateOffsets();
        if (m_mascot->state->dead) {
            forceRepaint = true;
            new_frame.name = "";
            new_sound = "";
            m_mascot->state->active_sound_changed = true;
            markForDeletion();
        }
        if (offsetsChanged || forceRepaint) {
            repaint();
            update();
        }
        if (m_mascot->state->active_sound_changed) {
            m_sounds.stop();
            if (!new_sound.empty()) {
                m_sounds.play(QString::fromStdString(new_sound));
            }
        }
        else if (!m_sounds.playing()) {
            m_mascot->state->active_sound.clear();
        }

        // Update inspector
        if (m_inspector != nullptr && m_inspector->isVisible()) {
            m_inspector->tick();
        }

        // Update speech bubble position
        if (m_speechBubble != nullptr && m_speechBubble->isActive()) {
            QPoint anchorPos = mapToGlobal(QPoint(width() / 2, 0));
            m_speechBubble->updatePosition(anchorPos);
        }
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Tick failed for mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString() << "\": " << ex.what();
        markForDeletion();
        close();
    }
    catch (...) {
        APP_LOG_ERROR("mascot") << "Tick failed for mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString()
            << "\" with unknown exception";
        markForDeletion();
        close();
    }
}

void ShijimaWidget::contextMenuClosed(QCloseEvent *event) {
    m_contextMenuVisible = false;
}

void ShijimaWidget::showContextMenu(QPoint const& pos) {
    m_contextMenuVisible = true;
    ShijimaContextMenu *menu = new ShijimaContextMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(pos);
}

ShijimaWidget::~ShijimaWidget() {
    cancelMouseInteraction();
    if (m_speechBubble != nullptr) {
        m_speechBubble->hideBubble();
        delete m_speechBubble;
        m_speechBubble = nullptr;
    }
    clearDragTargetReference();
    if (m_inspector != nullptr) {
        m_inspector->close();
        delete m_inspector;
    }
    setDragTarget(nullptr);
}
