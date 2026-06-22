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

#include "shijima-qt/AssetLoader.hpp"
#include "shijima-qt/Asset.hpp"
#include "shijima-qt/DefaultMascot.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/SecurityLimits.hpp"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

AssetLoader::AssetLoader() {}

static AssetLoader *m_defaultLoader = nullptr;

AssetLoader *AssetLoader::defaultLoader() {
    if (m_defaultLoader == nullptr) {
        m_defaultLoader = new AssetLoader;
    }
    return m_defaultLoader;
}

void AssetLoader::finalize() {
    if (m_defaultLoader != nullptr) {
        delete m_defaultLoader;
        m_defaultLoader = nullptr;
    }
}

Asset const& AssetLoader::loadAsset(QString path) {
    path = QDir::cleanPath(path);
    if (!m_assets.contains(path)) {
        Asset &asset = m_assets[path];
        QImage image;
        bool loaded = false;
        if (path.startsWith("@")) {
            auto filename = path.sliced(path.lastIndexOf('/') + 1)
                .toStdString();
            if (defaultMascot.count(filename) == 1) {
                auto &file = defaultMascot.at(filename);
                loaded = image.loadFromData((const uchar *)file.first,
                    (int)file.second);
            }
        }
        else {
            QFileInfo info(path);
            if (info.exists() && info.size() >= 0 &&
                static_cast<std::uint64_t>(info.size()) <=
                    SecurityLimits::kMascotSingleFileMaxBytes)
            {
                QImageReader reader(path);
                QSize imageSize = reader.size();
                std::uint64_t pixels = imageSize.isValid()
                    ? static_cast<std::uint64_t>(imageSize.width())
                        * static_cast<std::uint64_t>(imageSize.height())
                    : 0;
                if (imageSize.isValid() &&
                    pixels <= SecurityLimits::kMascotImageMaxPixels)
                {
                    loaded = reader.read(&image);
                }
                else {
                    APP_LOG_ERROR("asset") << "Rejected oversized image asset path=\""
                        << path.toStdString() << "\"";
                }
            }
            else {
                APP_LOG_ERROR("asset") << "Rejected oversized image file path=\""
                    << path.toStdString() << "\"";
            }
        }
        if (!loaded || image.isNull()) {
            APP_LOG_ERROR("asset") << "Failed to load image asset path=\""
                << path.toStdString() << "\"; using transparent placeholder";
            image = QImage { 1, 1, QImage::Format_ARGB32_Premultiplied };
            image.fill(Qt::transparent);
        }
        asset.setImage(image);
    }
    return m_assets[path];
}

void AssetLoader::unloadAssets(QString root) {
    root = QDir::cleanPath(root);
    for (auto &path : m_assets.keys()) {
        if (path.startsWith(root)) {
            m_assets.remove(path);
        }
    }
}
