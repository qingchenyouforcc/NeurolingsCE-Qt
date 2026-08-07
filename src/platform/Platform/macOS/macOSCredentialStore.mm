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

#include "macOSCredentialStore.hpp"

#include <QString>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace {

QString keychainErrorText(OSStatus status) {
    return QStringLiteral("macOS Keychain error %1").arg(
        static_cast<int>(status));
}

NSDictionary *baseQuery(QString const& service, QString const& account) {
    return @{
        (__bridge id) kSecClass: (__bridge id) kSecClassGenericPassword,
        (__bridge id) kSecAttrService: service.toNSString(),
        (__bridge id) kSecAttrAccount: account.toNSString(),
    };
}

}

CredentialStore::Status macOSCredentialStore::save(
    QString const& service, QString const& account, QString const& secret,
    QString *error)
{
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef) baseQuery(
        service, account));
    if (status != errSecSuccess && status != errSecItemNotFound) {
        if (error != nullptr) {
            *error = keychainErrorText(status);
        }
        return Status::Error;
    }
    NSData *data = [secret.toNSString() dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableDictionary *query = [NSMutableDictionary
        dictionaryWithDictionary:baseQuery(service, account)];
    query[(__bridge id) kSecValueData] = data;
    status = SecItemAdd((__bridge CFDictionaryRef) query, nullptr);
    if (status != errSecSuccess) {
        if (error != nullptr) {
            *error = keychainErrorText(status);
        }
        return Status::Error;
    }
    return Status::Ok;
}

CredentialStore::Status macOSCredentialStore::load(
    QString const& service, QString const& account, QString *secret,
    QString *error)
{
    NSMutableDictionary *query = [NSMutableDictionary
        dictionaryWithDictionary:baseQuery(service, account)];
    query[(__bridge id) kSecReturnData] = @YES;
    query[(__bridge id) kSecMatchLimit] = (__bridge id) kSecMatchLimitOne;
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query,
        &result);
    if (status == errSecItemNotFound) {
        return Status::Unavailable;
    }
    if (status != errSecSuccess) {
        if (error != nullptr) {
            *error = keychainErrorText(status);
        }
        return Status::Error;
    }
    NSData *data = (__bridge_transfer NSData *) result;
    if (secret != nullptr) {
        *secret = QString::fromUtf8(
            static_cast<char const *>(data.bytes),
            static_cast<qsizetype>(data.length));
    }
    return Status::Ok;
}

CredentialStore::Status macOSCredentialStore::remove(
    QString const& service, QString const& account, QString *error)
{
    OSStatus status = SecItemDelete(
        (__bridge CFDictionaryRef) baseQuery(service, account));
    if (status != errSecSuccess && status != errSecItemNotFound) {
        if (error != nullptr) {
            *error = keychainErrorText(status);
        }
        return Status::Error;
    }
    return Status::Ok;
}

CredentialStore::Status macOSCredentialStore::removeAll(
    QString const& service, QString *error)
{
    NSDictionary *query = @{
        (__bridge id) kSecClass: (__bridge id) kSecClassGenericPassword,
        (__bridge id) kSecAttrService: service.toNSString(),
    };
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef) query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
        if (error != nullptr) {
            *error = keychainErrorText(status);
        }
        return Status::Error;
    }
    return Status::Ok;
}

bool macOSCredentialStore::isAvailable() const {
    return true;
}
