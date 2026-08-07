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

#include "shijima-qt/CredentialStore.hpp"

class macOSCredentialStore final : public CredentialStore {
public:
    Status save(QString const& service, QString const& account,
        QString const& secret, QString *error) override;
    Status load(QString const& service, QString const& account,
        QString *secret, QString *error) override;
    Status remove(QString const& service, QString const& account,
        QString *error) override;
    Status removeAll(QString const& service,
        QString *error) override;
    bool isAvailable() const override;
};
