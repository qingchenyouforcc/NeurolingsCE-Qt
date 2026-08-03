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

#include <memory>

#include <QList>
#include <QMap>

#include "Platform/ActiveWindowObserver.hpp"
#include <shijima/mascot/environment.hpp>

class QScreen;
class QWidget;
class MascotSessionStore;

class ManagerEnvironmentController {
public:
    Platform::ActiveWindowObserver& windowObserver();

    double userScale() const;
    double detachThreshold() const;
    void setUserScale(double scale);
    void setDetachThreshold(double threshold);
    void setAllowsBreeding(bool allowsBreeding);
    void setAllowsWindowPushing(bool allowsWindowPushing);
    bool allowsWindowPushing() const;

    void screenAdded(QScreen *screen);
    void screenRemoved(QScreen *screen, QScreen *primaryScreen,
        MascotSessionStore& sessions);
    void updateScreen(QScreen *screen, QWidget *sandboxWidget,
        bool windowedMode);
    void updateAll(QWidget *sandboxWidget, bool windowedMode,
        QList<QScreen *> const& screens);

    std::shared_ptr<shijima::mascot::environment> environmentForScreen(
        QScreen *screen) const;
    QScreen *screenForEnvironment(shijima::mascot::environment *environment) const;
    void resetScales();

private:
    Platform::ActiveWindow m_previousWindow;
    Platform::ActiveWindow m_currentWindow;
    Platform::ActiveWindowObserver m_windowObserver;
    double m_userScale = 1.0;
    double m_detachThreshold = 30.0;
    bool m_allowsWindowPushing = false;
    QMap<QScreen *, std::shared_ptr<shijima::mascot::environment>> m_env;
    QMap<shijima::mascot::environment *, QScreen *> m_reverseEnv;
};
