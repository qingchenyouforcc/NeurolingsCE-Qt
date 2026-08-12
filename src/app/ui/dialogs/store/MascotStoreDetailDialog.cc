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
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTextBrowser>

MascotStoreDetailDialog::MascotStoreDetailDialog(
    MascotStoreEntry const& entry, QWidget *parent):
    QDialog(parent)
{
    setWindowTitle(tr("Mascot Details"));
    setMinimumSize(520, 420);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("<h2>%1 <small>v%2</small></h2>")
        .arg(entry.name.toHtmlEscaped(), entry.version.toHtmlEscaped()),
        this);
    title->setAccessibleName(entry.name);
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
    meta->setTextFormat(Qt::RichText);
    meta->setWordWrap(true);
    layout->addWidget(meta);

    auto *summary = new QLabel(entry.summary, this);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    auto *description = new QTextBrowser(this);
    description->setAccessibleName(tr("Mascot description"));
    description->setOpenExternalLinks(true);
    description->setPlainText(entry.summary);
    layout->addWidget(description, 1);

    auto *closeButton = new QPushButton(tr("Close"), this);
    closeButton->setAccessibleName(closeButton->text());
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
}
