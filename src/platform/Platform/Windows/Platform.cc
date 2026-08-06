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

#include "../Platform.hpp"
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace Platform {

void initialize(int argc, char **argv) {
    // Only redirect stdout/stderr to files when the process has a valid
    // console attached. GUI processes launched without a console can have
    // invalid inherited handles, and freopen would corrupt CRT stream state.
    if (GetConsoleWindow() != nullptr) {
        freopen("shijima_stdout.txt", "a", stdout);
        freopen("shijima_stderr.txt", "a", stderr);
    }
}

void showOnAllDesktops(QWidget *widget) {
    HWND window = (HWND)widget->winId();
    LONG_PTR exstyle = GetWindowLongPtr(window, GWL_EXSTYLE);
    if (exstyle != 0) {
        exstyle |= WS_EX_TOOLWINDOW;
        SetWindowLongPtr(window, GWL_EXSTYLE, exstyle);
    }
}

void refreshTopmost(QWidget *widget) {
    HWND window = (HWND)widget->winId();
    if (window == NULL || !IsWindow(window)) {
        return;
    }

    // Reassert the topmost z-order without stealing focus so the mascot
    // can move back above the taskbar after the taskbar is clicked.
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

bool useWindowMasks() {
    return false;
}

bool supportsWindowPushing() {
    return true;
}

bool pushWindow(ActiveWindow const& activeWindow, double dx, double dy) {
    if (!activeWindow.available || activeWindow.nativeHandle == 0 ||
        !std::isfinite(dx) || !std::isfinite(dy))
    {
        return false;
    }

    // Keep package-provided action values bounded before they reach the Win32
    // API.  The default action uses a small impulse; a larger value is almost
    // certainly malformed mascot data rather than a useful user action.
    dx = std::clamp(dx, -2000.0, 2000.0);
    dy = std::clamp(dy, -2000.0, 2000.0);

    HWND window = reinterpret_cast<HWND>(activeWindow.nativeHandle);
    if (!IsWindow(window) || !IsWindowVisible(window) || IsIconic(window) ||
        GetForegroundWindow() != window)
    {
        return false;
    }

    DWORD pid = 0;
    if (GetWindowThreadProcessId(window, &pid) == 0 ||
        pid == GetCurrentProcessId())
    {
        return false;
    }

    RECT rect;
    if (!GetWindowRect(window, &rect)) {
        return false;
    }

    UINT dpi = GetDpiForWindow(window);
    if (dpi == 0) {
        dpi = 96;
    }
    double physicalScale = static_cast<double>(dpi) / 96.0;
    int offsetX = static_cast<int>(std::lround(dx * physicalScale));
    int offsetY = static_cast<int>(std::lround(dy * physicalScale));
    if (offsetX == 0 && offsetY == 0) {
        return false;
    }

    return SetWindowPos(window, nullptr,
        rect.left + offsetX, rect.top + offsetY, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER) != 0;
}

}
