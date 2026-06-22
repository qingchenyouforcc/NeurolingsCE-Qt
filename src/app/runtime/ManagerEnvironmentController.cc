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

#include "ManagerEnvironmentController.hpp"

#include "MascotSessionStore.hpp"
#include "ManagerRuntimeHelpers.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"

#include <algorithm>
#include <cmath>

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

Platform::ActiveWindowObserver& ManagerEnvironmentController::windowObserver() {
    return m_windowObserver;
}

double ManagerEnvironmentController::userScale() const {
    return m_userScale;
}

double ManagerEnvironmentController::detachThreshold() const {
    return m_detachThreshold;
}

void ManagerEnvironmentController::setUserScale(double scale) {
    if (!std::isfinite(scale)) {
        scale = 1.0;
    }
    // Clamp to a sane range so a bad settings value cannot flatten the
    // environment or explode mascot physics.
    m_userScale = std::clamp(scale, 0.1, 10.0);
}

void ManagerEnvironmentController::setDetachThreshold(double threshold) {
    if (!std::isfinite(threshold)) {
        threshold = 30.0;
    }
    // The detach threshold controls how fast a moving window must be before we
    // treat it as "actually moving" rather than jitter.
    m_detachThreshold = std::max(0.0, threshold);
}

void ManagerEnvironmentController::setAllowsBreeding(bool allowsBreeding) {
    // Propagate the flag to every environment so spawned mascots see the same
    // policy regardless of which screen they live on.
    for (auto &env : m_env) {
        env->allows_breeding = allowsBreeding;
    }
}

void ManagerEnvironmentController::screenAdded(QScreen *screen) {
    if (m_env.contains(screen)) {
        return;
    }

    // Each screen gets its own environment snapshot because taskbar geometry,
    // cursor position, and active window bounds are screen-specific.
    auto env = std::make_shared<shijima::mascot::environment>();
    m_env[screen] = env;
    m_reverseEnv[env.get()] = screen;
    auto primary = QGuiApplication::primaryScreen();
    if (screen != primary && m_env.contains(primary)) {
        m_env[screen]->allows_breeding = m_env[primary]->allows_breeding;
    }
}

void ManagerEnvironmentController::screenRemoved(QScreen *screen,
    QScreen *primaryScreen, MascotSessionStore& sessions)
{
    if (!m_env.contains(screen) || screen == nullptr) {
        return;
    }

    // When a screen disappears, move every mascot back to the primary
    // environment so they do not keep stale pointers to the removed screen.
    auto primaryEnv = m_env.value(primaryScreen);
    for (auto &mascot : sessions.mascots()) {
        mascot->setEnv(primaryEnv);
        mascot->mascot().reset_position();
    }
    auto removedEnv = m_env.value(screen);
    if (removedEnv != nullptr) {
        m_reverseEnv.remove(removedEnv.get());
    }
    m_env.remove(screen);
}

void ManagerEnvironmentController::updateScreen(QScreen *screen,
    QWidget *sandboxWidget, bool windowedMode)
{
    if (!m_env.contains(screen)) {
        return;
    }

    auto &env = m_env[screen];
    QRect geometry, available;
    QPoint cursor;
    if (screen == nullptr) {
        // Windowed mode uses the sandbox widget geometry instead of a real
        // desktop screen so the simulation still has a coordinate frame.
        if (sandboxWidget != nullptr) {
            geometry = sandboxWidget->geometry();
            cursor = sandboxWidget->cursor().pos() - geometry.topLeft();
            geometry.setCoords(0, 0, geometry.width(), geometry.height());
            available = geometry;
        }
        else {
            APP_LOG_WARN("environment") << "Windowed environment requested before sandbox widget was initialized";
        }
    }
    else {
        cursor = QCursor::pos();
        geometry = screen->geometry();
        available = screen->availableGeometry();
    }

    int taskbarHeight = geometry.bottom() - available.bottom();
    int statusBarHeight = available.top() - geometry.top();
    if (taskbarHeight < 0) {
        taskbarHeight = 0;
    }
    if (statusBarHeight < 0) {
        statusBarHeight = 0;
    }

    env->screen = { (double)geometry.top() + statusBarHeight,
        (double)geometry.right(),
        (double)geometry.bottom(),
        (double)geometry.left() };
    // The floor excludes taskbars while the ceiling and work area still use
    // the full screen geometry for physics and collision behavior.
    env->floor = { (double)geometry.bottom() - taskbarHeight,
        (double)geometry.left(), (double)geometry.right() };
    env->work_area = { (double)geometry.top(),
        (double)geometry.right(),
        (double)geometry.bottom() - taskbarHeight,
        (double)geometry.left() };
    env->ceiling = { (double)geometry.top(), (double)geometry.left(),
        (double)geometry.right() };
    if (!windowedMode && m_currentWindow.available &&
        std::fabs(m_currentWindow.x) > 1 && std::fabs(m_currentWindow.y) > 1)
    {
        // Active window bounds are exposed as a separate interactive element so
        // mascots can react to moving foreground windows.
        env->active_ie = { m_currentWindow.y,
            m_currentWindow.x + m_currentWindow.width,
            m_currentWindow.y + m_currentWindow.height,
            m_currentWindow.x };
        if (m_previousWindow.available &&
            m_previousWindow.uid == m_currentWindow.uid)
        {
            double previousRight = m_previousWindow.x + m_previousWindow.width;
            double previousBottom = m_previousWindow.y + m_previousWindow.height;
            double currentRight = m_currentWindow.x + m_currentWindow.width;
            double currentBottom = m_currentWindow.y + m_currentWindow.height;
            env->active_ie.set_edge_offsets(
                m_currentWindow.x - m_previousWindow.x,
                currentRight - previousRight,
                m_currentWindow.y - m_previousWindow.y,
                currentBottom - previousBottom);

            if (m_detachThreshold > 0) {
                double speed = std::sqrt(env->active_ie.dx * env->active_ie.dx
                    + env->active_ie.dy * env->active_ie.dy);
                double upperBound = m_detachThreshold * 3.0;
                if (speed >= upperBound) {
                    // Very fast movement behaves like the window vanished rather
                    // than continuing to drag a huge interaction box around.
                    env->active_ie = { -50, -50, -50, -50 };
                }
                else if (speed > m_detachThreshold) {
                    // Gradually attenuate the interaction box so medium-speed
                    // window motion still influences mascots, just less strongly.
                    double ratio = 1.0 - (speed - m_detachThreshold)
                        / (upperBound - m_detachThreshold);
                    env->active_ie.dx *= ratio;
                    env->active_ie.dy *= ratio;
                    env->active_ie.left_dx *= ratio;
                    env->active_ie.right_dx *= ratio;
                    env->active_ie.top_dy *= ratio;
                    env->active_ie.bottom_dy *= ratio;
                }
            }
        }
    }
    else {
        env->active_ie = { -50, -50, -50, -50 };
    }

    int x = cursor.x(), y = cursor.y();
    env->cursor = { (double)x, (double)y, x - env->cursor.x, y - env->cursor.y };
    env->subtick_count = ShijimaManagerRuntimeInternal::kSubtickCount;
    // User scale is inverted because the mascot engine treats larger values as
    // smaller physical units.
    double userScale = std::isfinite(m_userScale) && m_userScale > 0
        ? m_userScale
        : 1.0;
    env->set_scale(1.0 / std::sqrt(userScale));
}

void ManagerEnvironmentController::updateAll(QWidget *sandboxWidget,
    bool windowedMode, QList<QScreen *> const& screens)
{
    m_currentWindow = m_windowObserver.getActiveWindow();
    if (windowedMode) {
        updateScreen(nullptr, sandboxWidget, windowedMode);
    }
    else {
        for (auto screen : screens) {
            updateScreen(screen, sandboxWidget, windowedMode);
        }
    }
    m_previousWindow = m_currentWindow;
}

std::shared_ptr<shijima::mascot::environment>
ManagerEnvironmentController::environmentForScreen(QScreen *screen) const
{
    return m_env.value(screen);
}

QScreen *ManagerEnvironmentController::screenForEnvironment(
    shijima::mascot::environment *environment) const
{
    return m_reverseEnv.value(environment, nullptr);
}

void ManagerEnvironmentController::resetScales() {
    for (auto &env : m_env) {
        env->reset_scale();
    }
}
