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

#include <QHash>
#include <QString>

#include <memory>

class CredentialStore {
public:
    enum class Status {
        Ok,
        Unavailable,
        Error,
    };

    virtual ~CredentialStore() = default;
    virtual Status save(QString const& service, QString const& account,
        QString const& secret, QString *error = nullptr) = 0;
    virtual Status load(QString const& service, QString const& account,
        QString *secret, QString *error = nullptr) = 0;
    virtual Status remove(QString const& service, QString const& account,
        QString *error = nullptr) = 0;
    // Removes every credential stored under a service (used by logout so
    // legacy/rotated accounts cannot survive).
    virtual Status removeAll(QString const& service,
        QString *error = nullptr) = 0;
    virtual bool isAvailable() const = 0;
};

class InMemoryCredentialStore : public CredentialStore {
public:
    Status save(QString const& service, QString const& account,
        QString const& secret, QString *error = nullptr) override;
    Status load(QString const& service, QString const& account,
        QString *secret = nullptr, QString *error = nullptr) override;
    Status remove(QString const& service, QString const& account,
        QString *error = nullptr) override;
    Status removeAll(QString const& service,
        QString *error = nullptr) override;
    bool isAvailable() const override;

private:
    QHash<QString, QString> m_secrets;
};

// Platform priority: Windows Credential Manager, macOS Keychain,
// Linux Secret Service (optional libsecret), else explicitly unavailable.
std::unique_ptr<CredentialStore> createPlatformCredentialStore(
    QString *error = nullptr);
