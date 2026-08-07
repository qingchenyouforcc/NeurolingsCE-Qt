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

#include "LinuxCredentialStore.hpp"

#if defined(SHIJIMA_WITH_LIBSECRET)
#include <libsecret/secret.h>
#endif

#include <QString>

namespace {

QString unavailableMessage() {
    return QStringLiteral(
        "Linux Secret Service is not built in; login persistence is "
        "unavailable (no plaintext fallback)");
}

}

CredentialStore::Status LinuxCredentialStore::save(
    QString const& service, QString const& account, QString const& secret,
    QString *error)
{
#if defined(SHIJIMA_WITH_LIBSECRET)
    Q_UNUSED(service);
    Q_UNUSED(account);
    Q_UNUSED(secret);
    if (error != nullptr) {
        *error = QStringLiteral("libsecret save is not wired yet");
    }
    return Status::Error;
#else
    Q_UNUSED(service);
    Q_UNUSED(account);
    Q_UNUSED(secret);
    if (error != nullptr) {
        *error = unavailableMessage();
    }
    return Status::Unavailable;
#endif
}

CredentialStore::Status LinuxCredentialStore::load(
    QString const& service, QString const& account, QString *secret,
    QString *error)
{
#if defined(SHIJIMA_WITH_LIBSECRET)
    Q_UNUSED(service);
    Q_UNUSED(account);
    if (secret != nullptr) {
        secret->clear();
    }
    return Status::Unavailable;
#else
    Q_UNUSED(service);
    Q_UNUSED(account);
    Q_UNUSED(secret);
    if (error != nullptr) {
        *error = unavailableMessage();
    }
    return Status::Unavailable;
#endif
}

CredentialStore::Status LinuxCredentialStore::remove(
    QString const& service, QString const& account, QString *error)
{
#if defined(SHIJIMA_WITH_LIBSECRET)
    Q_UNUSED(service);
    Q_UNUSED(account);
    return Status::Unavailable;
#else
    Q_UNUSED(service);
    Q_UNUSED(account);
    if (error != nullptr) {
        *error = unavailableMessage();
    }
    return Status::Unavailable;
#endif
}

CredentialStore::Status LinuxCredentialStore::removeAll(
    QString const& service, QString *error)
{
#if defined(SHIJIMA_WITH_LIBSECRET)
    Q_UNUSED(service);
    return Status::Unavailable;
#else
    Q_UNUSED(service);
    if (error != nullptr) {
        *error = unavailableMessage();
    }
    return Status::Unavailable;
#endif
}

bool LinuxCredentialStore::isAvailable() const {
#if defined(SHIJIMA_WITH_LIBSECRET)
    return true;
#else
    return false;
#endif
}
