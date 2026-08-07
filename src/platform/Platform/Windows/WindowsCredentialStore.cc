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

#include "WindowsCredentialStore.hpp"

#include <QString>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>

namespace {

QString windowsErrorText(DWORD code) {
    wchar_t *buffer = nullptr;
    DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0,
        reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    QString result;
    if (length > 0 && buffer != nullptr) {
        result = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    if (result.isEmpty()) {
        result = QStringLiteral("Windows credential error %1").arg(code);
    }
    return result;
}

}  // namespace

CredentialStore::Status WindowsCredentialStore::save(
    QString const& service, QString const& account, QString const& secret,
    QString *error)
{
    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName =
        const_cast<wchar_t *>(service.toStdWString().c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(
        secret.toUtf8().size());
    QByteArray secretBytes = secret.toUtf8();
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char *>(secretBytes.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName =
        const_cast<wchar_t *>(account.toStdWString().c_str());
    if (!CredWriteW(&credential, 0)) {
        if (error != nullptr) {
            *error = windowsErrorText(GetLastError());
        }
        return Status::Error;
    }
    return Status::Ok;
}

CredentialStore::Status WindowsCredentialStore::load(
    QString const& service, QString const& account, QString *secret,
    QString *error)
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(service.toStdWString().c_str(), CRED_TYPE_GENERIC, 0,
            &credential))
    {
        DWORD code = GetLastError();
        if (code == ERROR_NOT_FOUND) {
            return Status::Unavailable;
        }
        if (error != nullptr) {
            *error = windowsErrorText(code);
        }
        return Status::Error;
    }
    QByteArray bytes(reinterpret_cast<char const *>(credential->CredentialBlob),
        static_cast<qsizetype>(credential->CredentialBlobSize));
    QString storedAccount = QString::fromWCharArray(credential->UserName);
    CredFree(credential);
    if (!account.isEmpty() && storedAccount != account) {
        return Status::Unavailable;
    }
    if (secret != nullptr) {
        *secret = QString::fromUtf8(bytes);
    }
    return Status::Ok;
}

CredentialStore::Status WindowsCredentialStore::remove(
    QString const& service, QString const& account, QString *error)
{
    Q_UNUSED(account);
    if (!CredDeleteW(service.toStdWString().c_str(), CRED_TYPE_GENERIC, 0)) {
        DWORD code = GetLastError();
        if (code == ERROR_NOT_FOUND) {
            return Status::Ok;
        }
        if (error != nullptr) {
            *error = windowsErrorText(code);
        }
        return Status::Error;
    }
    return Status::Ok;
}

CredentialStore::Status WindowsCredentialStore::removeAll(
    QString const& service, QString *error)
{
    // Windows Credential Manager keys credentials by target name; the
    // service name is the target, so one delete covers every account.
    return remove(service, QString(), error);
}

bool WindowsCredentialStore::isAvailable() const {
    return true;
}
