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

#include <QElapsedTimer>
#include <QRegion>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <string>

#include <shijima/mascot/environment.hpp>
#include <shijima/mascot/manager.hpp>

#include "shijima-qt/Asset.hpp"
#include "shijima-qt/MascotData.hpp"
#include "shijima-qt/PlatformWidget.hpp"
#include "shijima-qt/SoundEffectManager.hpp"

class QCloseEvent;
class QMouseEvent;
class QPaintEvent;
class ShijimaContextMenu;
class ShimejiInspectorDialog;
class SpeechBubbleWidget;

class ShijimaWidget : public PlatformWidget<QWidget>
{
public:
    friend class ShijimaContextMenu;
    struct TickEnvironmentOverride {
        bool active = false;
        double floorY = 0.0;
        double workAreaBottom = 0.0;
    };
    explicit ShijimaWidget(MascotData *mascotData,
        std::unique_ptr<shijima::mascot::manager> mascot,
        int mascotId, bool windowedMode, QWidget *parent = nullptr);
    explicit ShijimaWidget(ShijimaWidget &old, bool windowedMode,
        QWidget *parent = nullptr);
    bool primeInitialFrame();
    void tick();
    bool pointInside(QPoint const& point);
    int mascotId() { return m_mascotId; }
    void showInspector();
    void showCodexNotification(QString const& message);
    void markForDeletion() { m_markedForDeletion = true; }
    bool markedForDeletion() const { return m_markedForDeletion; }
    bool inspectorVisible();
    bool paused() const { return m_paused || m_contextMenuVisible; }
    shijima::mascot::manager &mascot() {
        return *m_mascot;
    }
    void setEnv(std::shared_ptr<shijima::mascot::environment> env) {
        m_mascot->state->env = env;
    }
    std::shared_ptr<shijima::mascot::environment> env() {
        return m_mascot->state->env;
    }
    MascotData *mascotData() {
        return m_data;
    }
    QString const& mascotName() {
        return m_data->name();
    }
    ~ShijimaWidget();
    bool isFallThroughMode() const { return m_fallThroughMode; }
    bool dragging() const;
    void setDragging(bool dragging);
    TickEnvironmentOverride prepareTickEnvironmentOverride();
    void restoreTickEnvironmentOverride(TickEnvironmentOverride const& overrideState);
    void resetFallThroughTrackingIfDragged();
    void observeFallProgress(double anchorYBefore, double anchorYAfter);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    void setDragTarget(ShijimaWidget *target);
    QPoint envPosFromScreen(QPoint const& screenPos) const;
    void startHotspotHold(QPoint const& screenPos);
    bool stopHotspotHold();
    void maintainHotspotHold();
    bool isMirroredRender() const;
    void closeAction();
    void contextMenuClosed(QCloseEvent *);
    void showContextMenu(QPoint const&);
    bool updateOffsets();
    void showSpeechBubble();
    void handleClick(QPoint const& screenPos);
#ifdef __linux__
    QRegion m_windowMask;
#endif
    bool m_windowedMode;
    MascotData *m_data;
    ShimejiInspectorDialog *m_inspector;
    SoundEffectManager m_sounds;
    Asset const& getActiveAsset();
    ShijimaWidget *m_dragTarget = nullptr;
    ShijimaWidget **m_dragTargetPt = nullptr;
    std::unique_ptr<shijima::mascot::manager> m_mascot;
    QRect m_imageRect;
    QPoint m_anchorInWindow;
    double m_drawScale = 1.0;
    QPoint m_drawOrigin;
    int m_windowHeight;
    int m_windowWidth;
    bool m_visible;
    bool m_contextMenuVisible = false;
    bool m_paused = false;
    bool m_markedForDeletion = false;
    int m_mascotId;
    // Fall-through tracking: when mascot falls 700+ pixels, it bypasses
    // the taskbar floor and lands at the absolute screen bottom.
    bool m_fallTracking = false;
    double m_fallStartY = 0.0;
    bool m_fallThroughMode = false;
    // Speech bubble click tracking
    SpeechBubbleWidget *m_speechBubble = nullptr;
    QPoint m_lastPressGlobalPos;
    QElapsedTimer m_pressElapsedTimer;
    QTimer m_clickResetTimer;
    std::string m_hotspotHoldBehavior;
    QPoint m_hotspotHoldPressGlobalPos;
    bool m_hotspotHoldTriggered = false;
    int m_clickCount = 0;
};
