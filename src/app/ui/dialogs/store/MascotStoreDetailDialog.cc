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

#include "MascotStoreDetailDialog.hpp"

#include "shijima-qt/MascotStoreIndex.hpp"

#include <QBoxLayout>
#include <QColor>
#include <QGuiApplication>
#include <QPainter>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QStyleOptionFocusRect>

#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaTheme.h"

namespace {

void drawStoreDetailFocusFrame(QWidget *widget)
{
    if (!widget->hasFocus()) {
        return;
    }

    QStyleOptionFocusRect option;
    option.initFrom(widget);
    option.rect = widget->rect().adjusted(3, 3, -3, -3);
    QPainter painter(widget);
    widget->style()->drawPrimitive(
        QStyle::PE_FrameFocusRect, &option, &painter, widget);
}

class StoreDetailPushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawStoreDetailFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(StoreDetailPushButton)
};

void configureStoreDetailButton(QPushButton *button)
{
    button->setMinimumHeight(38);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(button->text());
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAutoDefault(false);
    button->setDefault(false);
}

QRect storeDetailAvailableGeometry(QWidget *widget)
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

void sizeStoreDetailDialog(QDialog *dialog, QLayout *layout)
{
    layout->activate();
    QRect available = storeDetailAvailableGeometry(dialog);
    int availableWidth = qMax(1, available.width() - 32);
    int availableHeight = qMax(1, available.height() - 32);
    int maxWidth = qMin(640, availableWidth);
    int maxHeight = qMin(560, availableHeight);
    int minWidth = qMin(440, maxWidth);
    int minHeight = qMin(320, maxHeight);
    QSize hint = layout->sizeHint();
    QSize preferred(
        qBound(minWidth, qMax(minWidth, hint.width()), maxWidth),
        qBound(minHeight, qMax(minHeight, hint.height()), maxHeight));
    dialog->setMinimumSize(minWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    dialog->resize(preferred);
}

}

MascotStoreDetailDialog::MascotStoreDetailDialog(
    MascotStoreEntry const& entry, QWidget *parent):
    QDialog(parent)
{
    setObjectName(QStringLiteral("MascotStoreDetailDialog"));
    setWindowTitle(tr("Mascot Details"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("<h2>%1 <small>v%2</small></h2>")
        .arg(entry.name.toHtmlEscaped(), entry.version.toHtmlEscaped()),
        this);
    title->setObjectName(QStringLiteral("storeDetailTitle"));
    title->setAccessibleName(entry.name);
    title->setWordWrap(true);
    layout->addWidget(title);

    QStringList authorLogins;
    for (auto const& author : entry.authors) {
        authorLogins.append(author);
    }
    auto *meta = new QLabel(tr(
        "License: %1<br>Authors: %2<br>Minimum client: %3<br>"
        "Package size: %4")
        .arg(entry.license.toHtmlEscaped(),
             authorLogins.join(QStringLiteral(", ")).toHtmlEscaped(),
             entry.minimumNeurolingsCEVersion.toHtmlEscaped(),
             QLocale().formattedDataSize(entry.download.size)),
        this);
    meta->setObjectName(QStringLiteral("storeDetailMeta"));
    meta->setTextFormat(Qt::RichText);
    meta->setWordWrap(true);
    layout->addWidget(meta);

    auto *descriptionTitle = new QLabel(tr("Description"), this);
    descriptionTitle->setObjectName(QStringLiteral("storeDetailDescriptionTitle"));
    descriptionTitle->setAccessibleName(tr("Description"));
    layout->addWidget(descriptionTitle);

    auto *description = new ElaPlainTextEdit(this);
    description->setObjectName(QStringLiteral("storeDetailDescription"));
    description->setAccessibleName(tr("Mascot description"));
    description->setReadOnly(true);
    description->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    description->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    description->setMinimumHeight(84);
    description->setMaximumHeight(260);
    description->setPlainText(entry.summary);
    layout->addWidget(description);

    auto *closeButton = new StoreDetailPushButton(tr("Close"), this);
    configureStoreDetailButton(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    setTabOrder(description, closeButton);

    auto applyTheme = [this, description]() {
        auto mode = eTheme->getThemeMode();
        QColor surface = ElaThemeColor(mode, DialogBase);
        QColor field = ElaThemeColor(mode, DialogLayoutArea);
        QColor border = ElaThemeColor(mode, BasicBorder);
        QColor text = ElaThemeColor(mode, BasicText);
        QColor muted = ElaThemeColor(mode, BasicDetailsText);
        QColor disabled = ElaThemeColor(mode, BasicTextDisable);
        setStyleSheet(QString(
            "#MascotStoreDetailDialog { background-color: %1; color: %4; }"
            "#MascotStoreDetailDialog QLabel { color: %4; background: transparent; }"
            "#storeDetailMeta, #storeDetailDescriptionTitle { color: %5; }"
            "#storeDetailDescription { background-color: %2; color: %4;"
            " border: 1px solid %3; border-radius: 8px; padding: 8px; }"
        ).arg(surface.name(QColor::HexArgb), field.name(QColor::HexArgb),
            border.name(QColor::HexArgb), text.name(QColor::HexArgb),
            muted.name(QColor::HexArgb)));

        QPalette palette = description->palette();
        palette.setColor(QPalette::Base, field);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Highlight,
            ElaThemeColor(mode, PrimaryNormal));
        palette.setColor(QPalette::HighlightedText, text);
        palette.setColor(QPalette::Disabled, QPalette::Base,
            ElaThemeColor(mode, BasicDisable));
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        description->setPalette(palette);
    };
    applyTheme();
    connect(eTheme, &ElaTheme::themeModeChanged, this, applyTheme);
    sizeStoreDetailDialog(this, layout);
}
