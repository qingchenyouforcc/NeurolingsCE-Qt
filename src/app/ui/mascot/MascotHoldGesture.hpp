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

#include <QEvent>

#include <cstdint>

namespace shijima::ui {

// The gesture policy is kept independent of QWidget so the interaction
// boundaries can be regression-tested without constructing a mascot window.
struct MascotHoldGesture {
    static constexpr std::int64_t kTriggerAfterMs = 260;
    static constexpr int kMovementTolerancePx = 12;
    static constexpr int kClickMovementTolerancePx = 6;
    static constexpr std::int64_t kClickDurationMs = 400;

    static constexpr bool withinMovementTolerance(int manhattanDistance) {
        return manhattanDistance >= 0 &&
            manhattanDistance <= kMovementTolerancePx;
    }

    static constexpr bool reachesLongPress(std::int64_t elapsedMs,
        int maxManhattanDistance)
    {
        return elapsedMs >= kTriggerAfterMs &&
            withinMovementTolerance(maxManhattanDistance);
    }

    static constexpr bool qualifiesAsClick(std::int64_t elapsedMs,
        int maxManhattanDistance)
    {
        return elapsedMs >= 0 && elapsedMs <= kClickDurationMs &&
            maxManhattanDistance >= 0 &&
            maxManhattanDistance <= kClickMovementTolerancePx;
    }

    static constexpr bool leftButtonIsDown(Qt::MouseButtons buttons) {
        return (buttons & Qt::LeftButton) != Qt::NoButton;
    }

    // A non-focusable mascot can legitimately receive FocusOut or
    // WindowDeactivate while the OS still owns the left-button grab.  Those
    // notifications must not terminate a hold; an explicit grab loss or
    // widget teardown remains terminal, and a deactivation after the button
    // is no longer down is treated as a lost release.
    static constexpr bool cancelsForEvent(QEvent::Type type,
        Qt::MouseButtons buttons)
    {
        switch (type) {
        case QEvent::UngrabMouse:
        case QEvent::Hide:
        case QEvent::Close:
            return true;
        case QEvent::ApplicationDeactivate:
        case QEvent::WindowDeactivate:
        case QEvent::FocusOut:
            return !leftButtonIsDown(buttons);
        default:
            return false;
        }
    }
};

}
