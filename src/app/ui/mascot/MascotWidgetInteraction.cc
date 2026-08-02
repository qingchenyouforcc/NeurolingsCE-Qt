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

#include <QCursor>
#include <QMouseEvent>
#include <QSettings>
#include <QVariant>

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/ui/widgets/SpeechBubbleWidget.hpp"

void ShijimaWidget::setDragTarget(ShijimaWidget *target) {
    if (target == m_dragTarget) {
        return;
    }
    if (m_dragTarget != nullptr) {
        m_dragTarget->stopHotspotHold();
        m_dragTarget->m_dragTargetPt = nullptr;
    }
    if (target != nullptr) {
        if (target->m_dragTargetPt != nullptr) {
            m_dragTarget = nullptr;
            return;
        }
        m_dragTarget = target;
        m_dragTarget->m_dragTargetPt = &m_dragTarget;
    }
    else {
        m_dragTarget = nullptr;
    }
}

void ShijimaWidget::mousePressEvent(QMouseEvent *event) {
    auto pos = event->pos();
    if (m_dragTarget != nullptr) {
        m_dragTarget->m_mascot->state->dragging = false;
    }
    if (pointInside(pos)) {
        setDragTarget(this);
    }
    else {
        QPoint envPos;
        if (m_windowedMode) {
            envPos = mapToParent(pos);
        }
        else {
            envPos = mapToGlobal(pos);
        }
        ShijimaWidget *target = ShijimaManager::defaultManager()->hitTest(envPos);
        setDragTarget(target);
        if (target == nullptr) {
            event->ignore();
            return;
        }
    }
    if (m_dragTarget == nullptr) {
        event->ignore();
        return;
    }
    if (!m_windowedMode) {
        Platform::refreshTopmost(m_dragTarget);
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        m_dragTarget->m_mascot->state->dragging = true;
        // Record press info for click detection
        m_lastPressGlobalPos = mapToGlobal(pos);
        m_pressElapsedTimer.start();
        m_dragTarget->startHotspotHold(m_lastPressGlobalPos);
    }
    else if (event->button() == Qt::MouseButton::RightButton) {
        auto screenPos = mapToGlobal(pos);
        m_dragTarget->showContextMenu(screenPos);
        setDragTarget(nullptr);
    }
}

void ShijimaWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    mousePressEvent(event);
    if (event->button() != Qt::MouseButton::LeftButton ||
        m_dragTarget == nullptr)
    {
        return;
    }

    auto targetEnvironment = m_dragTarget->env();
    if (targetEnvironment == nullptr || !targetEnvironment->allows_breeding) {
        return;
    }

    ShijimaManager::defaultManager()->spawn(
        m_dragTarget->mascotName().toStdString());
}

void ShijimaWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragTarget != nullptr) {
        auto currentGlobalPos = mapToGlobal(event->pos());
        if ((currentGlobalPos - m_lastPressGlobalPos).manhattanLength() > 12) {
            m_dragTarget->stopHotspotHold();
        }
    }
}

void ShijimaWidget::closeAction() {
    close();
}

void ShijimaWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragTarget == nullptr) {
        return;
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        // Detect click vs drag: small movement + short duration
        auto releaseGlobalPos = mapToGlobal(event->pos());
        int distance = (releaseGlobalPos - m_lastPressGlobalPos).manhattanLength();
        qint64 elapsed = m_pressElapsedTimer.elapsed();
        ShijimaWidget *clickTarget = m_dragTarget;
        bool hotspotHoldTriggered = clickTarget->stopHotspotHold();

        m_dragTarget->m_mascot->state->dragging = false;
        setDragTarget(nullptr);

        // Click threshold: less than 6px movement and less than 400ms
        if (!hotspotHoldTriggered && distance <= 6 && elapsed <= 400) {
            clickTarget->handleClick(releaseGlobalPos);
        }
    }
}

QPoint ShijimaWidget::envPosFromScreen(QPoint const& screenPos) const {
    QPoint envPos = screenPos;
    if (m_windowedMode && parentWidget() != nullptr) {
        envPos = parentWidget()->mapFromGlobal(screenPos);
    }
    return envPos;
}

void ShijimaWidget::startHotspotHold(QPoint const& screenPos) {
    QPoint envPos = envPosFromScreen(screenPos);
    m_hotspotHoldBehavior = m_mascot->hotspot_behavior({
        (double)envPos.x(), (double)envPos.y() });
    m_hotspotHoldTriggered = false;
    m_hotspotHoldPressGlobalPos = screenPos;
}

bool ShijimaWidget::stopHotspotHold() {
    bool triggered = m_hotspotHoldTriggered;
    m_hotspotHoldBehavior.clear();
    m_hotspotHoldTriggered = false;
    return triggered;
}

void ShijimaWidget::maintainHotspotHold() {
    if (m_hotspotHoldBehavior.empty()) {
        return;
    }
    if ((QCursor::pos() - m_hotspotHoldPressGlobalPos).manhattanLength() > 12) {
        stopHotspotHold();
        return;
    }
    if (m_pressElapsedTimer.elapsed() < 260) {
        return;
    }
    auto active = m_mascot->active_behavior();
    if (active == nullptr || active->name != m_hotspotHoldBehavior) {
        m_mascot->next_behavior(m_hotspotHoldBehavior);
    }
    else {
        m_mascot->prefer_next_behavior(m_hotspotHoldBehavior);
    }
    m_hotspotHoldTriggered = true;
}

void ShijimaWidget::handleClick(QPoint const& screenPos) {
    QPoint envPos = envPosFromScreen(screenPos);
    if (m_mascot->trigger_hotspot({ (double)envPos.x(), (double)envPos.y() })) {
        return;
    }

    m_clickCount++;
    m_clickResetTimer.start(500); // Reset click count after 500ms of no clicks

    // Trigger speech bubble when click count reaches threshold
    QSettings bubbleSettings("pixelomer", "Shijima-Qt");
    int threshold = bubbleSettings.value("speechBubbleClickCount", 1).toInt();
    if (m_clickCount == threshold) {
        showSpeechBubble();
    }
}

void ShijimaWidget::showSpeechBubble() {
    // Check if speech bubbles are enabled in settings
    QSettings settings("pixelomer", "Shijima-Qt");
    bool enabled = settings.value("speechBubbleEnabled",
        QVariant::fromValue(true)).toBool();
    if (!enabled) {
        return;
    }

    // Get random text
    QString text = SpeechBubbleWidget::randomBubbleText(m_data->path());

    // Create bubble widget if needed
    if (m_speechBubble == nullptr) {
        m_speechBubble = new SpeechBubbleWidget();
    }

    // Calculate anchor position (top-center of the mascot widget in screen coords)
    QPoint anchorPos = mapToGlobal(QPoint(width() / 2, 0));

    m_speechBubble->showBubble(text, anchorPos);
}

void ShijimaWidget::showCodexNotification(QString const& message) {
    if (m_speechBubble == nullptr) {
        m_speechBubble = new SpeechBubbleWidget();
    }
    QPoint anchorPos = mapToGlobal(QPoint(width() / 2, 0));
    m_speechBubble->showCodexBubble(message, anchorPos);
}
