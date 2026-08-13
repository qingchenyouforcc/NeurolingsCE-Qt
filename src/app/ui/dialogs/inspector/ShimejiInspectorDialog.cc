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

#include "shijima-qt/ui/dialogs/inspector/ShimejiInspectorDialog.hpp"
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"
#include <QFrame>
#include <QFormLayout>
#include <QGuiApplication>
#include <QPalette>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>
#include "ElaScrollArea.h"
#include "ElaTheme.h"

ShimejiInspectorDialog::ShimejiInspectorDialog(ShijimaWidget *parent):
    ElaDialog(parent), m_formLayout(nullptr)
{
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setIsFixedSize(false);
    setMinimumSize(440, 240);
    setWindowTitle(tr("Inspector — %1").arg(parent->mascotName()));
    auto *content = new QWidget(this);
    m_formLayout = new QFormLayout(content);
    m_formLayout->setFormAlignment(Qt::AlignLeft);
    m_formLayout->setLabelAlignment(Qt::AlignRight);
    m_formLayout->setHorizontalSpacing(16);
    m_formLayout->setVerticalSpacing(5);
    m_formLayout->setContentsMargins(2, 2, 2, 2);
    auto *scroll = new ElaScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidgetResizable(true);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scroll->setWidget(content);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 16, 22, 16);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(scroll);

    auto applyTheme = [this](ElaThemeType::ThemeMode mode) {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window,
            ElaThemeColor(mode, DialogBase));
        palette.setColor(QPalette::WindowText,
            ElaThemeColor(mode, BasicText));
        palette.setColor(QPalette::Text,
            ElaThemeColor(mode, BasicText));
        this->setPalette(palette);
    };
    applyTheme(eTheme->getThemeMode());
    connect(eTheme, &ElaTheme::themeModeChanged, this, applyTheme);
    connect(this, &ElaDialog::closeButtonClicked, this, &QDialog::reject);

    registerRows();
    content->adjustSize();
    rootLayout->activate();
    adjustSize();
    QRect available;
    if (auto *screen = this->screen(); screen != nullptr) {
        available = screen->availableGeometry();
    }
    else if (auto *screen = QGuiApplication::primaryScreen(); screen != nullptr) {
        available = screen->availableGeometry();
    }
    if (available.isEmpty()) {
        available = QRect(0, 0, 1280, 720);
    }
    int maxWidth = qMax(440, qMin(760, available.width() - 48));
    int maxHeight = qMax(240, qMin(560, qRound(available.height() * 0.72)));
    setMaximumSize(maxWidth, maxHeight);
    int naturalScrollHeight = qBound(180, content->sizeHint().height(),
        qMin(420, qRound(available.height() * 0.52)));
    scroll->setMinimumHeight(naturalScrollHeight);
    scroll->setMaximumHeight(naturalScrollHeight);
    QSize desired = rootLayout->sizeHint();
    desired.rheight() += 48;
    desired.setWidth(qMax(desired.width(), 440));
    desired.setHeight(qMax(desired.height(), 240));
    resize(qBound(440, desired.width(), maxWidth),
        qBound(240, desired.height(), maxHeight));
}

ShijimaWidget *ShimejiInspectorDialog::shijimaParent() {
    return static_cast<ShijimaWidget *>(parent());
}

void ShimejiInspectorDialog::tick() {
    for (auto &callback : m_tickCallbacks) {
        callback();
    }
}

#include "ShimejiInspectorDialog.moc"
