#pragma once

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

#include <QString>
#include <functional>
#include <string>
#include <vector>

#include "ElaDialog.h"

class ShijimaWidget;
class QFormLayout;

namespace shijima {
    namespace mascot {
        class manager;
    }
}

class ShimejiInspectorDialog : public ElaDialog {
    Q_OBJECT
private:
    std::vector<std::function<void()>> m_tickCallbacks;
    QFormLayout *m_formLayout;
    void registerRows();
    void addRow(QString const& label,
        std::function<std::string(shijima::mascot::manager &)> tick);
public:
    ShijimaWidget *shijimaParent();
    ShimejiInspectorDialog(ShijimaWidget *parent);
    void tick();
};
