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

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "../../runtime/ManagerRuntimeHelpers.hpp"
#include "../ManagerUiState.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"

namespace {

QString createTr(char const *sourceText, int n = -1)
{
    return QCoreApplication::translate("ShijimaManager", sourceText, nullptr, n);
}

QString localizedPackageMessage(QString const& message)
{
    if (message == QStringLiteral("Missing actions.xml")) {
        return createTr("Missing actions.xml");
    }
    if (message == QStringLiteral("Missing behaviors.xml")) {
        return createTr("Missing behaviors.xml");
    }
    if (message == QStringLiteral("Missing img/*.png")) {
        return createTr("Missing img/*.png");
    }
    if (message == QStringLiteral("Missing info.json; fallback metadata will be generated")) {
        return createTr("Missing info.json; fallback metadata will be generated");
    }
    return message;
}

void configureCreateButton(ElaPushButton *button)
{
    button->setMinimumHeight(34);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

ElaText *makeCreateTitle(QWidget *parent, QString const& text)
{
    auto *title = new ElaText(text, parent);
    title->setTextPixelSize(20);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("#ElaText { background-color: transparent; border: none; }"));
    return title;
}

QLabel *makeCreateDescription(QWidget *parent, QString const& text)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setProperty("muted", true);
    return label;
}

QFrame *makeCreatePanel(QWidget *parent)
{
    auto mode = eTheme->getThemeMode();
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("CreatePanel"));
    panel->setStyleSheet(QString(
        "#CreatePanel {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel[muted=\"true\"] { color: %3; }"
    ).arg(ElaThemeColor(mode, WindowBase).name(),
        ElaThemeColor(mode, BasicBorder).name(),
        ElaThemeColor(mode, BasicDetailsText).name()));
    return panel;
}

QString candidateDetails(LegacyMascotCandidate const& candidate)
{
    QStringList lines;
    if (!candidate.metadata.version.isEmpty()) {
        lines.append(createTr("Version: %1").arg(candidate.metadata.version));
    }
    if (!candidate.metadata.author.isEmpty()) {
        lines.append(createTr("Author: %1").arg(candidate.metadata.author));
    }
    lines.append(candidate.convertible
        ? createTr("Ready to convert.")
        : createTr("Cannot convert until the missing content is fixed."));
    for (auto const& warning : candidate.warnings) {
        lines.append(localizedPackageMessage(warning));
    }
    for (auto const& error : candidate.errors) {
        lines.append(localizedPackageMessage(error));
    }
    return lines.join(QStringLiteral("\n"));
}

QString resultSummary(QList<LegacyMascotConversionResult> const& results)
{
    QStringList lines;
    int successCount = 0;
    for (auto const& result : results) {
        if (result.ok) {
            ++successCount;
            lines.append(createTr("Created: %1").arg(result.packagePath));
        }
        else {
            lines.append(createTr("Failed: %1 - %2")
                .arg(result.name.isEmpty() ? createTr("(unknown)") : result.name,
                    result.errorMessage));
        }
    }
    if (results.isEmpty()) {
        return createTr("No mascots were converted.");
    }
    lines.prepend(createTr("Converted %n mascot(s).", successCount));
    return lines.join(QStringLiteral("\n"));
}

}

void ShijimaManager::setupCreatePage()
{
    m_ui->createPage = new QWidget(this);
    auto *root = new QVBoxLayout(m_ui->createPage);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    root->addWidget(makeCreateTitle(m_ui->createPage, tr("Create Mascot Package")));
    root->addWidget(makeCreateDescription(m_ui->createPage,
        tr("Check a Shimeji zip archive, choose the mascots to convert, and write .mascot packages to a folder you choose.")));

    auto *inputPanel = makeCreatePanel(m_ui->createPage);
    auto *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(14, 12, 14, 12);
    inputLayout->setSpacing(8);

    auto *zipRow = new QHBoxLayout;
    auto *zipEdit = new QLineEdit(inputPanel);
    zipEdit->setReadOnly(true);
    zipEdit->setPlaceholderText(tr("Choose a .zip archive"));
    auto *chooseZipButton = new ElaPushButton(tr("Choose Zip..."), inputPanel);
    chooseZipButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    configureCreateButton(chooseZipButton);
    zipRow->addWidget(zipEdit, 1);
    zipRow->addWidget(chooseZipButton);
    inputLayout->addLayout(zipRow);

    auto *checkButton = new ElaPushButton(tr("Check Content"), inputPanel);
    checkButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    configureCreateButton(checkButton);
    inputLayout->addWidget(checkButton, 0, Qt::AlignLeft);

    root->addWidget(inputPanel);

    auto *candidatePanel = makeCreatePanel(m_ui->createPage);
    auto *candidateLayout = new QVBoxLayout(candidatePanel);
    candidateLayout->setContentsMargins(14, 12, 14, 12);
    candidateLayout->setSpacing(8);
    auto *statusLabel = makeCreateDescription(candidatePanel, tr("No archive checked yet."));
    candidateLayout->addWidget(statusLabel);
    auto *candidateList = new QListWidget(candidatePanel);
    candidateList->setMinimumHeight(150);
    candidateList->setSelectionMode(QListWidget::SingleSelection);
    candidateLayout->addWidget(candidateList, 1);
    auto *descriptionLabel = new QLabel(tr("Description"), candidatePanel);
    candidateLayout->addWidget(descriptionLabel);
    auto *descriptionText = new QPlainTextEdit(candidatePanel);
    descriptionText->setReadOnly(true);
    descriptionText->setMinimumHeight(72);
    descriptionText->setMaximumHeight(110);
    descriptionText->setPlaceholderText(tr("Select a mascot to view its description."));
    candidateLayout->addWidget(descriptionText);
    root->addWidget(candidatePanel, 1);

    connect(candidateList, &QListWidget::currentItemChanged, this,
        [this, descriptionText](QListWidgetItem *current) {
            if (current == nullptr) {
                descriptionText->clear();
                return;
            }
            QString description = current->data(Qt::UserRole + 1).toString();
            descriptionText->setPlainText(description.isEmpty()
                ? tr("No description provided.") : description);
        });

    auto *outputPanel = makeCreatePanel(m_ui->createPage);
    auto *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(14, 12, 14, 12);
    outputLayout->setSpacing(8);
    auto *outputRow = new QHBoxLayout;
    auto *outputEdit = new QLineEdit(outputPanel);
    outputEdit->setReadOnly(true);
    outputEdit->setPlaceholderText(tr("Choose an output folder"));
    auto *chooseOutputButton = new ElaPushButton(tr("Choose Folder..."), outputPanel);
    chooseOutputButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    configureCreateButton(chooseOutputButton);
    outputRow->addWidget(outputEdit, 1);
    outputRow->addWidget(chooseOutputButton);
    outputLayout->addLayout(outputRow);

    auto *convertButton = new ElaPushButton(tr("Generate .mascot"), outputPanel);
    convertButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    configureCreateButton(convertButton);
    outputLayout->addWidget(convertButton, 0, Qt::AlignLeft);

    auto *resultText = new QPlainTextEdit(outputPanel);
    resultText->setReadOnly(true);
    resultText->setMinimumHeight(90);
    resultText->setPlaceholderText(tr("Conversion results will appear here."));
    outputLayout->addWidget(resultText);
    root->addWidget(outputPanel);

    connect(chooseZipButton, &ElaPushButton::clicked, this, [this, zipEdit,
        candidateList, statusLabel, descriptionText, resultText]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Choose Shimeji Zip Archive"),
            QString(), tr("Zip Archives (*.zip);;All Files (*)"));
        if (path.isEmpty()) {
            return;
        }
        zipEdit->setText(path);
        candidateList->clear();
        descriptionText->clear();
        statusLabel->setText(tr("Archive selected. Run content check before generating."));
        resultText->clear();
    });

    connect(chooseOutputButton, &ElaPushButton::clicked, this, [this, outputEdit]() {
        QString path = QFileDialog::getExistingDirectory(this, tr("Choose Output Folder"));
        if (!path.isEmpty()) {
            outputEdit->setText(path);
        }
    });

    connect(checkButton, &ElaPushButton::clicked, this,
        [this, zipEdit, candidateList, statusLabel, descriptionText,
            checkButton, resultText]() {
            QString path = zipEdit->text().trimmed();
            if (path.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Choose a .zip archive first."));
                return;
            }
            checkButton->setEnabled(false);
            statusLabel->setText(tr("Checking archive content..."));
            candidateList->clear();
            descriptionText->clear();
            resultText->clear();

            QtConcurrent::run([path]() {
                return MascotPackage::analyzeLegacyArchive(path);
            }).then([this, candidateList, statusLabel, descriptionText,
                checkButton](LegacyArchiveAnalysis analysis) {
                ShijimaManagerRuntimeInternal::dispatchToMainThread(
                    [this, candidateList, statusLabel, descriptionText,
                        checkButton, analysis]() {
                        checkButton->setEnabled(true);
                        candidateList->clear();
                        if (analysis.candidates.isEmpty()) {
                            statusLabel->setText(analysis.errorMessage.isEmpty()
                                ? tr("No Shimeji mascots were found in the archive.")
                                : analysis.errorMessage);
                            return;
                        }

                        int convertibleCount = 0;
                        for (auto const& candidate : analysis.candidates) {
                            if (candidate.convertible) {
                                ++convertibleCount;
                            }
                            auto *item = new QListWidgetItem(candidate.metadata.name);
                            item->setData(Qt::UserRole, candidate.name);
                            item->setData(Qt::UserRole + 1,
                                candidate.metadata.description);
                            item->setToolTip(candidateDetails(candidate));
                            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                            item->setCheckState(candidate.convertible
                                ? Qt::Checked : Qt::Unchecked);
                            if (!candidate.convertible) {
                                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                            }
                            candidateList->addItem(item);
                        }

                        if (candidateList->count() > 0) {
                            candidateList->setCurrentRow(0);
                        }

                        statusLabel->setText(tr("Found %1 mascot(s), %2 ready to convert.")
                            .arg(analysis.candidates.size()).arg(convertibleCount));
                    });
            });
        });

    connect(convertButton, &ElaPushButton::clicked, this,
        [this, zipEdit, outputEdit, candidateList, convertButton, resultText]() {
            QString archivePath = zipEdit->text().trimmed();
            QString outputPath = outputEdit->text().trimmed();
            if (archivePath.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Choose a .zip archive first."));
                return;
            }
            if (outputPath.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Choose an output folder first."));
                return;
            }

            QStringList selectedNames;
            for (int i = 0; i < candidateList->count(); ++i) {
                auto *item = candidateList->item(i);
                if ((item->flags() & Qt::ItemIsEnabled) &&
                    item->checkState() == Qt::Checked)
                {
                    selectedNames.append(item->data(Qt::UserRole).toString());
                }
            }
            if (selectedNames.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Select at least one mascot to convert."));
                return;
            }

            convertButton->setEnabled(false);
            resultText->setPlainText(tr("Generating .mascot package(s)..."));
            QtConcurrent::run([archivePath, outputPath, selectedNames]() {
                return MascotPackage::writeLegacyArchiveSelectionAsPackages(
                    archivePath, outputPath, selectedNames);
            }).then([this, convertButton, resultText](QList<LegacyMascotConversionResult> results) {
                ShijimaManagerRuntimeInternal::dispatchToMainThread(
                    [convertButton, resultText, results]() {
                        convertButton->setEnabled(true);
                        resultText->setPlainText(resultSummary(results));
                    });
            });
        });

    addPageNode(tr("Create"), m_ui->createPage, ElaIconType::WandMagicSparkles);
}
