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

#include "MascotSessionStore.hpp"

#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"

std::list<ShijimaWidget *> const& MascotSessionStore::mascots() const {
    return m_mascots;
}

std::list<ShijimaWidget *>& MascotSessionStore::mascots() {
    return m_mascots;
}

std::map<int, ShijimaWidget *> const& MascotSessionStore::mascotsById() const {
    return m_mascotsById;
}

std::map<int, ShijimaWidget *>& MascotSessionStore::mascotsById() {
    return m_mascotsById;
}

bool MascotSessionStore::empty() const {
    return m_mascots.empty();
}

int MascotSessionStore::size() const {
    return static_cast<int>(m_mascots.size());
}

void MascotSessionStore::add(ShijimaWidget *mascot) {
    if (mascot == nullptr) {
        return;
    }
    // Keep the list for stable iteration order and the map for direct id lookups.
    m_mascots.push_back(mascot);
    index(mascot);
}

void MascotSessionStore::index(ShijimaWidget *mascot) {
    if (mascot == nullptr) {
        return;
    }
    m_mascotsById[mascot->mascotId()] = mascot;
}

void MascotSessionStore::removeIndex(int mascotId) {
    m_mascotsById.erase(mascotId);
    // CLI labels are tied to runtime ids, so clearing one should also clear the other.
    clearCliLabelForMascot(mascotId);
}

void MascotSessionStore::clear() {
    m_mascots.clear();
    m_mascotsById.clear();
    // Reset transient handles together with the backing session state.
    clearCliLabels();
}

void MascotSessionStore::markAllForDeletion() {
    // Deletion is deferred so the runtime can finish the current tick safely.
    for (auto mascot : m_mascots) {
        mascot->markForDeletion();
    }
}

void MascotSessionStore::markAllForDeletion(QString const& name) {
    for (auto mascot : m_mascots) {
        if (mascot->mascotName() == name) {
            mascot->markForDeletion();
        }
    }
}

void MascotSessionStore::markAllButOneForDeletion(ShijimaWidget *widget) {
    for (auto mascot : m_mascots) {
        if (widget == mascot) {
            continue;
        }
        mascot->markForDeletion();
    }
}

void MascotSessionStore::markAllButOneForDeletion(QString const& name) {
    bool foundOne = false;
    for (auto mascot : m_mascots) {
        if (mascot->mascotName() == name) {
            if (!foundOne) {
                foundOne = true;
                continue;
            }
            mascot->markForDeletion();
        }
    }
}

std::optional<int> MascotSessionStore::cliLabelForMascot(int mascotId) const {
    auto it = m_cliLabelByMascotId.constFind(mascotId);
    if (it == m_cliLabelByMascotId.cend()) {
        return std::nullopt;
    }
    return it.value();
}

std::optional<int> MascotSessionStore::mascotIdForCliLabel(int cliLabel) const {
    auto it = m_cliLabelToMascotId.constFind(cliLabel);
    if (it == m_cliLabelToMascotId.cend()) {
        return std::nullopt;
    }
    return it.value();
}

bool MascotSessionStore::assignCliLabel(int mascotId,
    std::optional<int> preferredLabel, int &assignedLabel, QString &errorMessage)
{
    if (m_mascotsById.count(mascotId) != 1) {
        errorMessage = QStringLiteral("No such mascot");
        return false;
    }
    if (auto existing = cliLabelForMascot(mascotId); existing.has_value()) {
        if (!preferredLabel.has_value() || preferredLabel.value() == existing.value()) {
            assignedLabel = existing.value();
            return true;
        }
        errorMessage = QStringLiteral("Mascot already has a different CLI label");
        return false;
    }

    int label = -1;
    if (preferredLabel.has_value()) {
        label = preferredLabel.value();
        if (label < 0) {
            errorMessage = QStringLiteral("CLI label must be greater than or equal to 0");
            return false;
        }
        if (m_cliLabelToMascotId.contains(label)) {
            errorMessage = QStringLiteral("CLI label is already in use");
            return false;
        }
    }
    else {
        // Auto-assign the first unused label so repeated summons get stable
        // handles without forcing the caller to manage ids.
        label = m_nextCliLabel;
        while (m_cliLabelToMascotId.contains(label)) {
            ++label;
        }
        m_nextCliLabel = label + 1;
    }

    m_cliLabelToMascotId[label] = mascotId;
    m_cliLabelByMascotId[mascotId] = label;
    assignedLabel = label;
    return true;
}

void MascotSessionStore::clearCliLabelForMascot(int mascotId) {
    auto it = m_cliLabelByMascotId.find(mascotId);
    if (it == m_cliLabelByMascotId.end()) {
        return;
    }
    // Remove both directions of the mapping together to avoid stale handles.
    int cliLabel = it.value();
    m_cliLabelByMascotId.erase(it);
    m_cliLabelToMascotId.remove(cliLabel);
}

void MascotSessionStore::clearCliLabels() {
    m_cliLabelToMascotId.clear();
    m_cliLabelByMascotId.clear();
    m_nextCliLabel = 0;
}
