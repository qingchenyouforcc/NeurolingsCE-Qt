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

#pragma once

#include <QString>
#include <QUrl>

// Public build-time configuration. Only public values belong here; secrets
// must never be compiled into the client.
#ifndef NEUROLINGSCE_MASCOT_INDEX_URL
#define NEUROLINGSCE_MASCOT_INDEX_URL ""
#endif

#ifndef NEUROLINGSCE_SUBMISSION_SERVICE_URL
#define NEUROLINGSCE_SUBMISSION_SERVICE_URL ""
#endif

#ifndef NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID
#define NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID ""
#endif

namespace MascotStoreConfig {

inline QString indexUrl() {
    return QStringLiteral(NEUROLINGSCE_MASCOT_INDEX_URL);
}

inline QString submissionServiceUrl() {
    return QStringLiteral(NEUROLINGSCE_SUBMISSION_SERVICE_URL);
}

inline QString githubLoginClientId() {
    // Client ID of the dedicated Login GitHub App only. The Publisher App
    // client ID and private key never appear in the desktop client.
    return QStringLiteral(NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID);
}

inline bool isIndexConfigured() {
    QUrl url(indexUrl());
    return url.isValid() && !url.isEmpty() &&
        (url.scheme() == QStringLiteral("https") ||
            url.scheme() == QStringLiteral("http"));
}

inline bool isLoginConfigured() {
    return !githubLoginClientId().trimmed().isEmpty();
}

inline bool isConfigured() {
    return isIndexConfigured();
}

}
