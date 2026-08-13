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

#include "shijima-qt/ui/dialogs/licenses/ShijimaLicensesDialog.hpp"
#include <QColor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>
#include "licenses_generated.hpp"
#include <QMargins>
#include <QPalette>
#include "ElaPushButton.h"
#include "ElaTheme.h"

namespace {

QRect licensesAvailableGeometry(QWidget *widget)
{
    QScreen *screen = widget->screen();
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr) {
        return screen->availableGeometry();
    }
    return QRect(0, 0, 1024, 768);
}

void sizeLicensesDialog(QDialog *dialog, QLayout *layout)
{
    layout->activate();
    QRect available = licensesAvailableGeometry(dialog);
    int maxWidth = qMax(1, qMin(900, available.width() - 32));
    int maxHeight = qMax(1, qMin(760, available.height() - 32));
    int minWidth = qMin(420, maxWidth);
    int minHeight = qMin(360, maxHeight);
    QSize hint = layout->sizeHint();
    QSize preferred(
        qBound(minWidth, qMax(minWidth, hint.width()), maxWidth),
        qBound(minHeight, qMax(minHeight, hint.height()), maxHeight));
    dialog->setMinimumSize(minWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    dialog->resize(preferred);
}

}

ShijimaLicensesDialog::ShijimaLicensesDialog(QWidget *parent): QDialog(parent) {
    setObjectName(QStringLiteral("ShijimaLicensesDialog"));
    auto windowLayout = new QVBoxLayout { this };
    m_textEdit.setParent(this);
    m_textEdit.setObjectName(QStringLiteral("licensesTextEdit"));
    m_textEdit.setAccessibleName(tr("Licenses"));
    m_textEdit.setReadOnly(true);
    m_textEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_textEdit.setPlainText(QString::fromStdString(shijima_licenses));
    setWindowTitle(tr("Licenses"));
    setLayout(windowLayout);
    windowLayout->setContentsMargins(QMargins { 16, 16, 16, 16 });
    windowLayout->addWidget(&m_textEdit);

    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 4, 0, 0);
    actionRow->addStretch();
    auto *closeButton = new ElaPushButton(tr("Close"), this);
    closeButton->setObjectName(QStringLiteral("licensesCloseButton"));
    closeButton->setAccessibleName(tr("Close"));
    closeButton->setFocusPolicy(Qt::StrongFocus);
    closeButton->setAutoDefault(false);
    closeButton->setDefault(false);
    closeButton->setMinimumHeight(34);
    actionRow->addWidget(closeButton);
    windowLayout->addLayout(actionRow);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    setTabOrder(&m_textEdit, closeButton);

    auto applyTheme = [this]() {
        auto themeMode = eTheme->getThemeMode();
        QColor dialogBg = ElaThemeColor(themeMode, DialogBase);
        QColor textColor = ElaThemeColor(themeMode, BasicText);
        QColor baseBg = ElaThemeColor(themeMode, BasicBase);
        QColor border = ElaThemeColor(themeMode, BasicBorder);
        QColor muted = ElaThemeColor(themeMode, BasicDetailsText);
        setStyleSheet(QString(
            "#ShijimaLicensesDialog { background-color: %1; color: %2; }"
            "#licensesTextEdit { background-color: %3; color: %2;"
            " border: 1px solid %4; border-radius: 8px; padding: 8px; }"
            "#licensesCloseButton { min-height: 34px; }"
            "#licensesCloseButton:focus { border: 2px solid %5; }"
        ).arg(dialogBg.name(QColor::HexArgb), textColor.name(QColor::HexArgb),
            baseBg.name(QColor::HexArgb), border.name(QColor::HexArgb),
            ElaThemeColor(themeMode, PrimaryNormal).name(QColor::HexArgb)));

        QPalette palette = m_textEdit.palette();
        palette.setColor(QPalette::Base, baseBg);
        palette.setColor(QPalette::Text, textColor);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Highlight,
            ElaThemeColor(themeMode, PrimaryNormal));
        palette.setColor(QPalette::HighlightedText, textColor);
        palette.setColor(QPalette::Disabled, QPalette::Base,
            ElaThemeColor(themeMode, BasicDisable));
        palette.setColor(QPalette::Disabled, QPalette::Text,
            ElaThemeColor(themeMode, BasicTextDisable));
        m_textEdit.setPalette(palette);
    };
    applyTheme();
    connect(eTheme, &ElaTheme::themeModeChanged, this, applyTheme);
    sizeLicensesDialog(this, windowLayout);
}

#include "ShijimaLicensesDialog.moc"
