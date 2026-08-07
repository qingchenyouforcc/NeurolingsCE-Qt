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

#include "shijima-qt/CredentialStore.hpp"

#if defined(Q_OS_WIN)
#include "Platform/Windows/WindowsCredentialStore.hpp"
#elif defined(Q_OS_MACOS)
#include "Platform/macOS/macOSCredentialStore.hpp"
#elif defined(Q_OS_LINUX)
#include "Platform/Linux/LinuxCredentialStore.hpp"
#else
#include "Platform/Stub/StubCredentialStore.hpp"
#endif

std::unique_ptr<CredentialStore> createPlatformCredentialStore(QString *error) {
#if defined(Q_OS_WIN)
    return std::make_unique<WindowsCredentialStore>();
#elif defined(Q_OS_MACOS)
    return std::make_unique<macOSCredentialStore>();
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxCredentialStore>();
#else
    if (error != nullptr) {
        *error = QStringLiteral(
            "This platform has no secure credential storage");
    }
    return std::make_unique<StubCredentialStore>();
#endif
}
