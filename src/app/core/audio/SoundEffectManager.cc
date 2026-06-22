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

#include "shijima-qt/SoundEffectManager.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/SafePath.hpp"

#if SHIJIMA_USE_QTMULTIMEDIA

#include <QFile>
#include <QDir>
#include <QSoundEffect>

void SoundEffectManager::play(QString const& name) {
    if (!m_loadedEffects.contains(name)) {
        APP_LOG_DEBUG("audio") << "Loading sound effect name=\""
            << name.toStdString() << "\"";
        QUrl url;
        for (QString &searchPath : searchPaths) {
            auto file = SafePath::safeChildPath(searchPath, name);
            if (file.has_value() && QFile::exists(file.value())) {
                url = QUrl::fromLocalFile(file.value());
                break;
            }
        }
        if (url.isEmpty()) {
            APP_LOG_WARN("audio") << "Sound effect not found name=\""
                << name.toStdString() << "\"";
            return;
        }
        QSoundEffect *effect = m_loadedEffects[name] = new QSoundEffect;
        effect->setSource(url);
        effect->setLoopCount(1);
        effect->setVolume(1.f);
        APP_LOG_INFO("audio") << "Sound effect loaded name=\""
            << name.toStdString() << "\" source=\""
            << url.toLocalFile().toStdString() << "\"";
    }
    stop();
    QSoundEffect *effect = m_loadedEffects[name];
    effect->play();
    m_activeEffect = effect;
    APP_LOG_DEBUG("audio") << "Sound effect play requested name=\""
        << name.toStdString() << "\"";
}

bool SoundEffectManager::playing() const {
    if (m_activeEffect != nullptr) {
        return m_activeEffect->isPlaying();
    }
    return false;
}

void SoundEffectManager::stop() {
    if (m_activeEffect != nullptr) {
        m_activeEffect->stop();
        m_activeEffect = nullptr;
        APP_LOG_DEBUG("audio") << "Active sound effect stopped";
    }
}

SoundEffectManager::~SoundEffectManager() {
    for (QSoundEffect *effect : m_loadedEffects) {
        delete effect;
    }
}

#else

void SoundEffectManager::play(QString const& name) {
    APP_LOG_DEBUG("audio") << "Ignoring sound effect because Qt Multimedia is disabled name=\""
        << name.toStdString() << "\"";
}
bool SoundEffectManager::playing() const { return true; }
void SoundEffectManager::stop() {}
SoundEffectManager::~SoundEffectManager() {}

#endif
