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

#include "shijima-qt/Secrets.hpp"

#include <QRegularExpression>

QString redactSensitiveText(QString const& text) {
    if (text.isEmpty()) {
        return text;
    }
    static QRegularExpression const tokenPattern(
        QStringLiteral(
            "(?i)(bearer\\s+|token\\s*=\\s*|access_token[\"']?\\s*[:=]\\s*"
            "[\"']?)([A-Za-z0-9_\\-\\.]{8,})"));
    static QRegularExpression const fieldPattern(
        QStringLiteral(
            "(?i)(\"?(?:access_token|refresh_token|github_token|client_secret"
            "|password)\"?\\s*[:=]\\s*[\"']?)[^,\\s\"'}]+"));
    QString result = text;
    result.replace(tokenPattern,
        QStringLiteral("\\1[REDACTED]"));
    result.replace(fieldPattern,
        QStringLiteral("\\1[REDACTED]"));
    return result;
}
