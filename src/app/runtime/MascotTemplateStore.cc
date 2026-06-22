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

#include "MascotTemplateStore.hpp"

#include "shijima-qt/MascotData.hpp"

#include <stdexcept>

MascotTemplateStore::MascotTemplateStore() = default;

MascotTemplateStore::~MascotTemplateStore() = default;

MascotData *MascotTemplateStore::registerTemplate(
    std::unique_ptr<MascotData> data)
{
    if (data == nullptr || !data->valid()) {
        throw std::runtime_error("registerTemplate() called with invalid data");
    }

    // Register with the shijima factory first so the template can be spawned
    // immediately after it enters the store.
    shijima::mascot::factory::tmpl tmpl;
    tmpl.actions_xml = data->actionsXML().toStdString();
    tmpl.behaviors_xml = data->behaviorsXML().toStdString();
    tmpl.name = data->name().toStdString();
    tmpl.path = data->path().toStdString();
    m_factory.register_template(tmpl);

    MascotData *raw = data.get();
    m_loadedMascots.insert(raw->name(), raw);
    m_loadedMascotsById.insert(raw->id(), raw);
    m_ownedTemplates.push_back(std::move(data));
    return raw;
}

std::unique_ptr<MascotData> MascotTemplateStore::takeTemplate(
    QString const& name)
{
    auto mapIt = m_loadedMascots.find(name);
    if (mapIt == m_loadedMascots.end()) {
        return {};
    }

    MascotData *raw = mapIt.value();
    // Deregister before removing ownership so the factory cannot hand out the
    // template after its backing object has gone away.
    m_factory.deregister_template(name.toStdString());
    m_loadedMascots.erase(mapIt);
    m_loadedMascotsById.remove(raw->id());

    for (auto it = m_ownedTemplates.begin(); it != m_ownedTemplates.end(); ++it) {
        if (it->get() != raw) {
            continue;
        }
        std::unique_ptr<MascotData> owned = std::move(*it);
        m_ownedTemplates.erase(it);
        return owned;
    }
    return {};
}

QMap<QString, MascotData *> const& MascotTemplateStore::loadedMascots() const {
    return m_loadedMascots;
}

QMap<int, MascotData *> const& MascotTemplateStore::loadedMascotsById() const {
    return m_loadedMascotsById;
}

QMap<QString, MascotData *>& MascotTemplateStore::loadedMascots() {
    return m_loadedMascots;
}

QMap<int, MascotData *>& MascotTemplateStore::loadedMascotsById() {
    return m_loadedMascotsById;
}

shijima::mascot::factory& MascotTemplateStore::factory() {
    return m_factory;
}

shijima::mascot::factory const& MascotTemplateStore::factory() const {
    return m_factory;
}
