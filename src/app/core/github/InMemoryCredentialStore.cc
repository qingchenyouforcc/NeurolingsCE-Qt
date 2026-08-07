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

CredentialStore::Status InMemoryCredentialStore::save(
    QString const& service, QString const& account, QString const& secret,
    QString *error)
{
    Q_UNUSED(error);
    m_secrets[service + QLatin1Char('/') + account] = secret;
    return Status::Ok;
}

CredentialStore::Status InMemoryCredentialStore::load(
    QString const& service, QString const& account, QString *secret,
    QString *error)
{
    Q_UNUSED(error);
    auto it = m_secrets.constFind(service + QLatin1Char('/') + account);
    if (it == m_secrets.constEnd()) {
        return Status::Unavailable;
    }
    if (secret != nullptr) {
        *secret = it.value();
    }
    return Status::Ok;
}

CredentialStore::Status InMemoryCredentialStore::remove(
    QString const& service, QString const& account, QString *error)
{
    Q_UNUSED(error);
    m_secrets.remove(service + QLatin1Char('/') + account);
    return Status::Ok;
}

CredentialStore::Status InMemoryCredentialStore::removeAll(
    QString const& service, QString *error)
{
    Q_UNUSED(error);
    QString prefix = service + QLatin1Char('/');
    for (auto it = m_secrets.begin(); it != m_secrets.end();) {
        if (it.key().startsWith(prefix)) {
            it = m_secrets.erase(it);
        } else {
            ++it;
        }
    }
    return Status::Ok;
}

bool InMemoryCredentialStore::isAvailable() const {
    return true;
}
