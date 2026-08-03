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

#include <cstdbool>
#include <QString>
#include <QtGlobal>

namespace Platform {

class ActiveWindow {
public:
    bool available;
    QString uid;
    long pid;
    double x, y, width, height;
    ActiveWindow(QString const& uid, long pid, double x, double y,
        double width, double height, quintptr nativeHandle = 0):
        available(true), uid(uid), pid(pid), x(x), y(y), width(width),
        height(height), nativeHandle(nativeHandle) {}
    // Platform-specific window handle.  It is intentionally opaque to the
    // runtime so non-Windows backends can keep this at zero.
    quintptr nativeHandle = 0;
    ActiveWindow(): available(false), nativeHandle(0) {}
};

}
