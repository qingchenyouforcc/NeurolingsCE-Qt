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

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

struct MascotStoreMedia {
    QUrl url;
    qint64 size = -1;
    QString sha256;
};

struct MascotStoreEntry {
    QString id;
    QString name;
    QString version;
    QString summary;
    QString license;
    QString minimumNeurolingsCEVersion;
    QStringList authors;
    QStringList maintainers;
    QStringList tags;
    QStringList categories;
    MascotStoreMedia download;
    MascotStoreMedia icon;
    QList<MascotStoreMedia> previews;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct MascotStoreIndex {
    int schemaVersion = 0;
    QString registry;
    QDateTime generatedAt;
    QList<MascotStoreEntry> entries;

    // Parses index-v1 bytes. Returns false and fills *error on malformed data.
    bool parse(QByteArray const& bytes, QString *error = nullptr);
    // Entries are kept sorted by id; deterministic for identical input.
    void sortEntries();

    static bool isValidVersion(QString const& version);
    static bool isNewerVersion(QString const& candidate, QString const& current);

    const MascotStoreEntry *findById(QString const& id) const;
    QList<MascotStoreEntry> filter(QString const& query,
        QStringList const& tagFilter) const;
};
