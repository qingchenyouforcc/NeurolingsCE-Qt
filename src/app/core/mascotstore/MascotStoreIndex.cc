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

#include "shijima-qt/MascotStoreIndex.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QVersionNumber>

#include <algorithm>

namespace {

QString const kSha256Pattern = QStringLiteral("^[0-9a-f]{64}$");

QDateTime parseIso(QString const& text) {
    return QDateTime::fromString(text, Qt::ISODate);
}

MascotStoreMedia parseMedia(QJsonValue const& value) {
    MascotStoreMedia media;
    if (!value.isObject()) {
        return media;
    }
    QJsonObject object = value.toObject();
    media.url = QUrl { object.value(QStringLiteral("url")).toString() };
    media.size = object.value(QStringLiteral("size")).toDouble(-1);
    media.sha256 = object.value(QStringLiteral("sha256")).toString().toLower();
    return media;
}

QStringList stringList(QJsonValue const& value) {
    QStringList result;
    if (!value.isArray()) {
        return result;
    }
    for (auto const& item : value.toArray()) {
        if (item.isString()) {
            result.append(item.toString());
        }
    }
    return result;
}

bool validSha256(QString const& sha) {
    return QRegularExpression(kSha256Pattern).match(sha).hasMatch();
}

bool isTrustedDownloadUrl(QUrl const& url) {
    if (url.scheme() == QStringLiteral("https")) {
        return true;
    }
    if (url.scheme() == QStringLiteral("http")) {
        QString host = url.host().toLower();
        return host == QStringLiteral("localhost") ||
            host == QStringLiteral("127.0.0.1") ||
            host == QStringLiteral("::1");
    }
    return false;
}

}  // namespace

bool MascotStoreIndex::parse(QByteArray const& bytes, QString *error) {
    auto fail = [&](QString const& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("Index is not valid JSON"));
    }
    QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
        return fail(QStringLiteral(
            "Unsupported index schema version; expected 1"));
    }
    MascotStoreIndex parsed;
    parsed.schemaVersion = 1;
    parsed.registry = root.value(QStringLiteral("registry")).toString();
    parsed.generatedAt = parseIso(
        root.value(QStringLiteral("generatedAt")).toString());
    if (!parsed.generatedAt.isValid()) {
        return fail(QStringLiteral("Index generatedAt is invalid"));
    }
    QJsonValue mascotsValue = root.value(QStringLiteral("mascots"));
    if (!mascotsValue.isArray()) {
        return fail(QStringLiteral("Index is missing the mascots array"));
    }
    for (auto const& item : mascotsValue.toArray()) {
        if (!item.isObject()) {
            return fail(QStringLiteral("Index contains a malformed mascot entry"));
        }
        QJsonObject object = item.toObject();
        MascotStoreEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.version = object.value(QStringLiteral("version")).toString();
        entry.summary = object.value(QStringLiteral("summary")).toString();
        entry.license = object.value(QStringLiteral("license")).toString();
        entry.minimumNeurolingsCEVersion = object.value(
            QStringLiteral("minimumNeurolingsCEVersion")).toString();
        entry.authors = stringList(object.value(QStringLiteral("authors")));
        entry.maintainers = stringList(object.value(QStringLiteral("maintainers")));
        entry.tags = stringList(object.value(QStringLiteral("tags")));
        entry.categories = stringList(object.value(QStringLiteral("categories")));
        entry.download = parseMedia(object.value(QStringLiteral("download")));
        entry.icon = parseMedia(object.value(QStringLiteral("icon")));
        for (auto const& preview : object.value(QStringLiteral("previews")).toArray()) {
            entry.previews.append(parseMedia(preview));
        }
        entry.createdAt = parseIso(
            object.value(QStringLiteral("createdAt")).toString());
        entry.updatedAt = parseIso(
            object.value(QStringLiteral("updatedAt")).toString());

        if (entry.id.isEmpty() || entry.name.isEmpty() ||
            !isValidVersion(entry.version) || entry.summary.isEmpty() ||
            !entry.download.url.isValid() ||
            !isTrustedDownloadUrl(entry.download.url) ||
            !validSha256(entry.download.sha256))
        {
            return fail(QStringLiteral(
                "Index contains a mascot entry with invalid required fields"));
        }
        parsed.entries.append(entry);
    }
    parsed.sortEntries();
    *this = parsed;
    return true;
}

void MascotStoreIndex::sortEntries() {
    std::sort(entries.begin(), entries.end(),
        [](MascotStoreEntry const& lhs, MascotStoreEntry const& rhs) {
            return QString::compare(lhs.id, rhs.id) < 0;
        });
}

bool MascotStoreIndex::isValidVersion(QString const& version) {
    QVersionNumber number = QVersionNumber::fromString(version);
    return !number.isNull() && number.majorVersion() >= 0;
}

bool MascotStoreIndex::isNewerVersion(QString const& candidate,
    QString const& current)
{
    QVersionNumber candidateNumber = QVersionNumber::fromString(candidate);
    QVersionNumber currentNumber = QVersionNumber::fromString(current);
    if (candidateNumber.isNull() || currentNumber.isNull()) {
        return false;
    }
    return candidateNumber > currentNumber;
}

const MascotStoreEntry *MascotStoreIndex::findById(QString const& id) const {
    for (auto const& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QList<MascotStoreEntry> MascotStoreIndex::filter(QString const& query,
    QStringList const& tagFilter) const
{
    QList<MascotStoreEntry> result;
    QStringList queryTerms = query.trimmed().split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (auto const& entry : entries) {
        if (!tagFilter.isEmpty()) {
            bool matched = false;
            for (auto const& tag : tagFilter) {
                if (entry.tags.contains(tag, Qt::CaseInsensitive) ||
                    entry.categories.contains(tag, Qt::CaseInsensitive))
                {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
        }
        if (!queryTerms.isEmpty()) {
            QString haystack = QStringLiteral("%1 %2 %3 %4")
                .arg(entry.name, entry.summary, entry.id, entry.authors.join(
                    QLatin1Char(' ')));
            bool matched = true;
            for (auto const& term : queryTerms) {
                if (!haystack.contains(term, Qt::CaseInsensitive)) {
                    matched = false;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
        }
        result.append(entry);
    }
    return result;
}
