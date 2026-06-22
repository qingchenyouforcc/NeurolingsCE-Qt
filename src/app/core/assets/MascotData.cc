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

#include "shijima-qt/MascotData.hpp"
#include "shijima-qt/AssetLoader.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/SecurityLimits.hpp"
#include <QDirIterator>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QTextStream>
#include <QDir>
#include "shijima-qt/DefaultMascot.hpp"
#include <stdexcept>
#include <cstdint>
#include <shijima/parser.hpp>

static QString readFile(QString const& file) {
    QFile f { file };
    if (!f.open(QFile::ReadOnly | QFile::Text))
        throw std::runtime_error("failed to open file: " + file.toStdString());
    QTextStream in(&f);
    return in.readAll(); 
}

static QImage loadPreviewImage(QString const& filePath) {
    QImageReader reader(filePath);
    QSize size = reader.size();
    if (!size.isValid()) {
        throw std::runtime_error("invalid preview image");
    }
    std::uint64_t width = static_cast<std::uint64_t>(size.width());
    std::uint64_t height = static_cast<std::uint64_t>(size.height());
    std::uint64_t pixels = width * height;
    if (width == 0 || height == 0 ||
        pixels > SecurityLimits::kMascotImageMaxPixels)
    {
        throw std::runtime_error("preview image is too large");
    }
    QImage frame;
    if (!reader.read(&frame) || frame.isNull()) {
        throw std::runtime_error("failed to load preview image");
    }
    return frame;
}

MascotData::MascotData(): m_valid(false) {}

MascotData::MascotData(QString const& path, int id):
    MascotData(path, path, id) {}

MascotData::MascotData(QString const& packagePath, QString const& cachePath, int id):
    m_path(cachePath), m_packagePath(packagePath),
    m_valid(true), m_id(id) 
{
    if (packagePath == "@") {
        m_metadata = MascotPackage::metadataFromJson(
            QByteArray(defaultMascot.at("info.json").first,
                (qsizetype)defaultMascot.at("info.json").second));
        m_name = m_metadata.name;
        m_behaviorsXML = QString { defaultMascot.at("behaviors.xml").first };
        m_actionsXML = QString { defaultMascot.at("actions.xml").first };
        m_path = "@";
        m_packagePath = "@";
        m_imgRoot = "@/img";
        m_valid = true;
        m_deletable = false;
        QImage frame;
        frame.loadFromData((const uchar *)defaultMascot.at("shime1.png").first,
            (int)defaultMascot.at("shime1.png").second);
        QImage preview = renderPreview(frame);
        m_preview = QPixmap::fromImage(preview);
        return;
    }
    m_deletable = true;
    QString errorMessage;
    if (!MascotPackage::extractPackage(packagePath, cachePath, errorMessage)) {
        throw std::runtime_error(errorMessage.toStdString());
    }
    QDir dir { cachePath };
    m_metadata = MascotPackage::metadataFromJson(
        readFile(dir.filePath("info.json")).toUtf8());
    m_name = m_metadata.name;
    m_behaviorsXML = readFile(dir.filePath("behaviors.xml"));
    m_actionsXML = readFile(dir.filePath("actions.xml"));
    {
        // this throws if the XMLs are invalid
        shijima::parser parser;
        parser.parse(m_actionsXML.toStdString(), m_behaviorsXML.toStdString());
    }
    dir.cd("img");
    m_imgRoot = QDir::cleanPath(cachePath + QDir::separator() + "img");
    QDirIterator iter { dir.absolutePath(), QDir::Files,
        QDirIterator::NoIteratorFlags };
    QList<QString> images;
    while (iter.hasNext()) {
        auto entry = iter.nextFileInfo();
        auto basename = entry.fileName();
        if (basename.endsWith(".png")) {
            images.append(basename);
        }
    }
    images.sort(Qt::CaseInsensitive);
    if (images.isEmpty()) {
        throw std::runtime_error("mascot package does not contain preview images");
    }
    QImage preview = renderPreview(loadPreviewImage(
        dir.absoluteFilePath(images[0])));
    m_preview = QPixmap::fromImage(preview);
}

QImage MascotData::renderPreview(QImage frame) {
    frame = frame.scaled({ 128, 128 }, Qt::KeepAspectRatio);
    QImage preview { 128, 128, QImage::Format_ARGB32_Premultiplied };
    preview.fill(Qt::transparent);
    QPainter painter { &preview };
    painter.setBackgroundMode(Qt::BGMode::TransparentMode);
    painter.drawImage(QPoint{ (128 - frame.width()) / 2, 0 }, frame);
    return preview;
}

QString const &MascotData::imgRoot() const {
    return m_imgRoot;
}

void MascotData::unloadCache() const {
    AssetLoader::defaultLoader()->unloadAssets(m_path);
}

bool MascotData::deletable() const {
    return m_deletable;
}

bool MascotData::valid() const {
    return m_valid;
}

QString const &MascotData::behaviorsXML() const {
    return m_behaviorsXML;
}

QString const &MascotData::actionsXML() const {
    return m_actionsXML;
}

QString const &MascotData::path() const {
    return m_path;
}

QString const &MascotData::packagePath() const {
    return m_packagePath;
}

QString const &MascotData::name() const {
    return m_name;
}

MascotMetadata const &MascotData::metadata() const {
    return m_metadata;
}

QIcon const &MascotData::preview() const {
    return m_preview;
}

int MascotData::id() const {
    return m_id;
}
