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
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSettings>
#include <QVariant>

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/ui/widgets/SpeechBubbleWidget.hpp"
#include "MascotHoldGesture.hpp"

void ShijimaWidget::setDragTarget(ShijimaWidget *target) {
    if (target == m_dragTarget) {
        return;
    }
    if (m_dragTarget != nullptr) {
        m_dragTarget->stopHotspotHold();
        m_dragTarget->m_leftPressActive = false;
        if (m_dragTarget->m_mascot != nullptr) {
            m_dragTarget->m_mascot->state->dragging = false;
        }
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

bool ShijimaWidget::event(QEvent *event) {
    if (shijima::ui::MascotHoldGesture::cancelsForEvent(
            event->type(), QGuiApplication::mouseButtons())) {
        cancelMouseInteraction();
    }
    return PlatformWidget<QWidget>::event(event);
}

void ShijimaWidget::clearDragTargetReference() {
    // When this widget is the target of another widget's press, the owner
    // stores its m_dragTarget address here.  Drop our back-reference first,
    // then clear the owner; setting our member to nullptr before
    // dereferencing also makes this safe when the owner is clearing the same
    // pair.
    ShijimaWidget **ownerTarget = m_dragTargetPt;
    m_dragTargetPt = nullptr;
    if (ownerTarget != nullptr) {
        *ownerTarget = nullptr;
    }
}

void ShijimaWidget::beginLeftPress(QPoint const& screenPos) {
    m_leftPressActive = true;
    m_lastPressGlobalPos = screenPos;
    m_pressMaxMovement = 0;
    m_pressElapsedTimer.start();
    startHotspotHold(screenPos);
    // A hotspot candidate is held outside the engine's dragging path until
    // the long-press threshold is reached.  Otherwise animation::tick()
    // would activate the hotspot immediately on the first GUI tick, turning a
    // short click into a long press.  A non-hotspot press keeps the existing
    // drag behavior from the start.
    m_mascot->state->dragging = m_hotspotHoldBehavior.empty();
}

void ShijimaWidget::cancelMouseInteraction() {
    // This object may be the target held by a different event receiver.  In
    // that case m_dragTarget is usually null, but leaving m_dragTargetPt
    // intact would keep the receiver's pointer occupied and turn its later
    // release into a no-op.
    clearDragTargetReference();
    if (m_dragTarget == nullptr) {
        // This can be called from a deactivation event after the target has
        // already destroyed itself.  Clear any local candidate state too.
        stopHotspotHold();
        m_leftPressActive = false;
        if (m_mascot != nullptr) {
            m_mascot->state->dragging = false;
        }
        return;
    }

    ShijimaWidget *target = m_dragTarget;
    target->stopHotspotHold();
    target->m_leftPressActive = false;
    if (target->m_mascot != nullptr) {
        target->m_mascot->state->dragging = false;
    }
    setDragTarget(nullptr);
}

void ShijimaWidget::mousePressEvent(QMouseEvent *event) {
    auto pos = event->pos();
    cancelMouseInteraction();
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
        // Keep the press clock and candidate movement on the mascot that is
        // actually being interacted with.  A transparent overlapping widget
        // can receive the event while hitTest() selects a different target.
        m_dragTarget->beginLeftPress(mapToGlobal(pos));
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
    if (m_dragTarget != nullptr && m_dragTarget->m_leftPressActive) {
        auto currentGlobalPos = mapToGlobal(event->pos());
        int distance = (currentGlobalPos - m_dragTarget->m_lastPressGlobalPos)
            .manhattanLength();
        m_dragTarget->m_pressMaxMovement = qMax(
            m_dragTarget->m_pressMaxMovement, distance);
        if (!shijima::ui::MascotHoldGesture::withinMovementTolerance(distance)) {
            m_dragTarget->stopHotspotHold();
            m_dragTarget->m_mascot->state->dragging = true;
        }
    }
}

void ShijimaWidget::closeAction() {
    close();
}

void ShijimaWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragTarget == nullptr || !m_dragTarget->m_leftPressActive) {
        return;
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        // Detect click vs drag: small movement + short duration
        auto releaseGlobalPos = mapToGlobal(event->pos());
        ShijimaWidget *clickTarget = m_dragTarget;
        int distance = (releaseGlobalPos - clickTarget->m_lastPressGlobalPos)
            .manhattanLength();
        clickTarget->m_pressMaxMovement = qMax(
            clickTarget->m_pressMaxMovement, distance);
        qint64 elapsed = clickTarget->m_pressElapsedTimer.elapsed();
        bool hotspotHoldTriggered = clickTarget->stopHotspotHold();

        clickTarget->m_mascot->state->dragging = false;
        setDragTarget(nullptr);

        // A click is only valid when the pointer stayed within the tighter
        // click tolerance for the entire press.  This prevents a drag that
        // briefly crossed the hold tolerance and then returned from becoming
        // a click on release.
        if (!hotspotHoldTriggered &&
            shijima::ui::MascotHoldGesture::qualifiesAsClick(
                elapsed, clickTarget->m_pressMaxMovement))
        {
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
    m_hotspotHoldPreferredNext = false;
    m_hotspotHoldPressGlobalPos = screenPos;
}

bool ShijimaWidget::stopHotspotHold() {
    bool triggered = m_hotspotHoldTriggered;
    std::string behavior = m_hotspotHoldBehavior;
    if (m_mascot != nullptr && !behavior.empty()) {
        // A release can arrive before the next engine tick.  In that window
        // next_behavior() may still be queued, so cancel only the request
        // created by this hold instead of allowing a post-release pat.
        if (m_mascot->state->queued_behavior == behavior) {
            m_mascot->state->queued_behavior.clear();
        }
        if (m_hotspotHoldPreferredNext) {
            // Restore the active behavior's own Add/NextBehaviorList rules;
            // prefer_next_behavior() is a temporary hold override.
            m_mascot->clear_preferred_next_behavior();
        }
    }
    m_hotspotHoldBehavior.clear();
    m_hotspotHoldTriggered = false;
    m_hotspotHoldPreferredNext = false;
    return triggered;
}

void ShijimaWidget::maintainHotspotHold() {
    if (!m_leftPressActive || m_hotspotHoldBehavior.empty()) {
        return;
    }
    if (!shijima::ui::MascotHoldGesture::leftButtonIsDown(
            QGuiApplication::mouseButtons())) {
        // A non-focusable tool window may lose its native grab without
        // delivering mouseReleaseEvent.  Treat the missing left button as a
        // release on the next GUI tick so a stale hold cannot restart later.
        cancelMouseInteraction();
        return;
    }
    int cursorDistance = (QCursor::pos() - m_hotspotHoldPressGlobalPos)
        .manhattanLength();
    if (!shijima::ui::MascotHoldGesture::withinMovementTolerance(cursorDistance)) {
        stopHotspotHold();
        m_mascot->state->dragging = true;
        return;
    }
    if (!shijima::ui::MascotHoldGesture::reachesLongPress(
        m_pressElapsedTimer.elapsed(), m_pressMaxMovement))
    {
        return;
    }

    auto active = m_mascot->active_behavior();
    auto const& queued = m_mascot->state->queued_behavior;
    if (active != nullptr && active->name == m_hotspotHoldBehavior) {
        // Keep the current patpat action's next selection pinned to itself so
        // the animation starts another round as soon as it completes.  This
        // does not activate a behavior immediately and is cleared on release
        // or cancellation.
        m_mascot->prefer_next_behavior(m_hotspotHoldBehavior);
        m_hotspotHoldPreferredNext = true;
    }
    else {
        // Avoid queueing the same behavior on every tick while the manager is
        // still completing the previous action.  Once the action changes, a
        // single queue request starts the next patpat cycle.
        if (queued != m_hotspotHoldBehavior) {
            m_mascot->next_behavior(m_hotspotHoldBehavior);
        }
    }
    // The first queue request may be selected by the engine on the following
    // tick; mark the gesture now so release suppresses the click path.
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
        connect(m_speechBubble, &SpeechBubbleWidget::codexActivated,
            this, []() {
                if (auto *manager = ShijimaManager::defaultManager()) {
                    manager->showCodexPage();
                }
            });
    }

    // Calculate anchor position (top-center of the mascot widget in screen coords)
    QPoint anchorPos = mapToGlobal(QPoint(width() / 2, 0));

    m_speechBubble->showBubble(text, anchorPos);
}

void ShijimaWidget::showCodexNotification(QString const& message,
    QString const& title)
{
    if (m_speechBubble == nullptr) {
        m_speechBubble = new SpeechBubbleWidget();
        connect(m_speechBubble, &SpeechBubbleWidget::codexActivated,
            this, []() {
                if (auto *manager = ShijimaManager::defaultManager()) {
                    manager->showCodexPage();
                }
            });
    }
    QPoint anchorPos = mapToGlobal(QPoint(width() / 2, 0));
    m_speechBubble->showCodexBubble(message, anchorPos, title);
}
