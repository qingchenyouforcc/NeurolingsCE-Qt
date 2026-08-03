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

#include "PrivateActiveWindowObserver.hpp"
#include <windows.h>
#include <process.h>
#include <cwchar>
#include <QString>
#include <string>

namespace Platform {

namespace {

bool isShellWindow(HWND window) {
    wchar_t className[64] = {};
    int length = GetClassNameW(window, className,
        static_cast<int>(sizeof(className) / sizeof(className[0])));
    if (length <= 0) {
        return false;
    }
    return wcscmp(className, L"Progman") == 0 ||
        wcscmp(className, L"WorkerW") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0 ||
        wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

}

PrivateActiveWindowObserver::PrivateActiveWindowObserver() {}

ActiveWindow PrivateActiveWindowObserver::getActiveWindow() {
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == NULL) {
        // Do not retain a previous target when the shell has no foreground
        // window.  A cached rectangle here is indistinguishable from a real
        // target to the mascot engine and makes it push empty space.
        return m_activeWindow = {};
    }
    DWORD newPid;
    if (GetWindowThreadProcessId(foregroundWindow, &newPid) == 0) {
        return m_activeWindow = {};
    }
    if ((long)newPid == _getpid()) {
        // The manager's own transparent/tool windows are not push targets.
        return m_activeWindow = {};
    }
    if (foregroundWindow == GetDesktopWindow() ||
        foregroundWindow == GetShellWindow() ||
        isShellWindow(foregroundWindow) ||
        !IsWindow(foregroundWindow) || !IsWindowVisible(foregroundWindow) ||
        IsIconic(foregroundWindow))
    {
        return m_activeWindow = {};
    }
    RECT rect;
    if (!GetWindowRect(foregroundWindow, &rect)) {
        return m_activeWindow = {};
    }
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return m_activeWindow = {};
    }
    UINT dpi = GetDpiForWindow(foregroundWindow);
    if (dpi == 0) {
        return m_activeWindow = {};
    }
    double scaleRatio = 96.0 / dpi;
    QString uid = QString::fromStdString(std::to_string(newPid) + "-"
        + std::to_string((unsigned long long)foregroundWindow));
    return m_activeWindow = { uid, (long)newPid,
        rect.left * scaleRatio,
        rect.top * scaleRatio,
        (rect.right - rect.left) * scaleRatio,
        (rect.bottom - rect.top) * scaleRatio,
        reinterpret_cast<quintptr>(foregroundWindow) };
}

}
