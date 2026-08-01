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
#include "shijima-qt/SecurityLimits.hpp"
#include "../ManagerUiState.hpp"

#include <QBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QFrame>
#include <QCoreApplication>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyleOptionFocusRect>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <cstdint>
#include <exception>
#include <utility>

#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"

namespace {

constexpr int kCompactCreateRowWidth = 520;
constexpr int kCandidateNameRole = Qt::UserRole;
constexpr int kCandidateInfoJsonRole = Qt::UserRole + 1;
constexpr int kCandidateInfoJsonValidRole = Qt::UserRole + 2;
constexpr int kCandidateInfoJsonDirtyRole = Qt::UserRole + 3;
constexpr int kCandidateDetailsRole = Qt::UserRole + 4;
constexpr int kCandidateInfoJsonPendingRole = Qt::UserRole + 5;
constexpr int kCandidateInfoJsonErrorRole = Qt::UserRole + 6;

QString createTr(char const *sourceText, int n = -1)
{
    return QCoreApplication::translate("ShijimaManager", sourceText, nullptr, n);
}

QString localizedMetadataReason(QString const& reason)
{
    if (reason == QStringLiteral("Invalid info.json")) {
        return createTr("Invalid info.json");
    }
    if (reason == QStringLiteral("info.json must contain a non-empty name")) {
        return createTr("info.json must contain a non-empty name");
    }
    return createTr("Invalid metadata");
}

QString infoJsonValidationError(QByteArray const& infoJson,
    MascotMetadata *parsedMetadata = nullptr)
{
    if (static_cast<std::uint64_t>(infoJson.size()) >
        SecurityLimits::kMascotSingleFileMaxBytes)
    {
        return createTr("Edited info.json is too large");
    }
    try {
        MascotMetadata metadata = MascotPackage::metadataFromJson(infoJson);
        if (!MascotPackage::isValidPackageName(metadata.name)) {
            return createTr("Edited info.json has an invalid package name");
        }
        if (parsedMetadata != nullptr) {
            *parsedMetadata = std::move(metadata);
        }
        return {};
    }
    catch (std::exception const& ex) {
        return localizedMetadataReason(QString::fromUtf8(ex.what()));
    }
    catch (...) {
        return createTr("Invalid metadata");
    }
}

QString localizedInfoJsonValidationError(QString const& error)
{
    if (error == QStringLiteral(
        "Edited info.json has an invalid package name"))
    {
        return createTr("Edited info.json has an invalid package name");
    }
    return localizedMetadataReason(error);
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
    if (message == QStringLiteral("Could not recognize this mascot in the archive")) {
        return createTr("Could not recognize this mascot in the archive");
    }
    if (message == QStringLiteral("Archive does not exist")) {
        return createTr("Archive does not exist");
    }
    if (message == QStringLiteral("Could not create temporary directory")) {
        return createTr("Could not create temporary directory");
    }
    if (message == QStringLiteral("Could not analyze archive")) {
        return createTr("Could not analyze archive");
    }
    if (message == QStringLiteral("No Shimeji mascots were found in the archive")) {
        return createTr("No Shimeji mascots were found in the archive");
    }
    if (message == QStringLiteral("No convertible mascots were found")) {
        return createTr("No convertible mascots were found");
    }
    if (message == QStringLiteral("Could not create output directory")) {
        return createTr("Could not create output directory");
    }
    if (message == QStringLiteral("Selected mascot was not found")) {
        return createTr("Selected mascot was not found");
    }
    if (message == QStringLiteral("Edited info.json is too large")) {
        return createTr("Edited info.json is too large");
    }
    if (message == QStringLiteral("Edited info.json is invalid")) {
        return createTr("Edited info.json is invalid");
    }
    if (message == QStringLiteral("Edited info.json has an invalid package name")) {
        return createTr("Edited info.json has an invalid package name");
    }
    if (message == QStringLiteral("Could not write edited info.json")) {
        return createTr("Could not write edited info.json");
    }
    if (message == QStringLiteral("Extracted archive directory is missing")) {
        return createTr("Extracted archive directory is missing");
    }
    if (message == QStringLiteral("Archive extracted an unsafe path")) {
        return createTr("Archive extracted an unsafe path");
    }
    if (message == QStringLiteral("Archive contains symbolic links")) {
        return createTr("Archive contains symbolic links");
    }
    if (message == QStringLiteral("Archive contains too many extracted files")) {
        return createTr("Archive contains too many extracted files");
    }
    if (message == QStringLiteral("Archive extracted data is too large")) {
        return createTr("Archive extracted data is too large");
    }
    if (message == QStringLiteral("Source mascot directory does not exist")) {
        return createTr("Source mascot directory does not exist");
    }
    if (message == QStringLiteral("Mascot package source is missing required files")) {
        return createTr("Mascot package source is missing required files");
    }
    if (message == QStringLiteral("Could not write package entry")) {
        return createTr("Could not write package entry");
    }
    if (message == QStringLiteral("Mascot package is too large for ZIP32 central directory")) {
        return createTr("Mascot package is too large for ZIP32 central directory");
    }
    if (message == QStringLiteral("Mascot package central directory is too large")) {
        return createTr("Mascot package central directory is too large");
    }
    if (message == QStringLiteral("Mascot package contains too many entries")) {
        return createTr("Mascot package contains too many entries");
    }
    if (message == QStringLiteral("Mascot package entry path is too long")) {
        return createTr("Mascot package entry path is too long");
    }
    if (message == QStringLiteral("Mascot package entry is too large for ZIP32")) {
        return createTr("Mascot package entry is too large for ZIP32");
    }
    if (message == QStringLiteral("Mascot package is too large for ZIP32 offsets")) {
        return createTr("Mascot package is too large for ZIP32 offsets");
    }
    QString const sizePrefix = QStringLiteral("Archive exceeds the maximum size of ");
    QString const sizeSuffix = QStringLiteral(" bytes");
    if (message.startsWith(sizePrefix) && message.endsWith(sizeSuffix)) {
        return createTr("Archive exceeds the maximum size of %1 bytes").arg(
            message.mid(sizePrefix.size(), message.size() - sizePrefix.size() -
                sizeSuffix.size()));
    }
    QString const extractedFilePrefix = QStringLiteral("Extracted file ");
    QString const extractedFileSuffix = QStringLiteral(" exceeds size limits");
    if (message.startsWith(extractedFilePrefix) &&
        message.endsWith(extractedFileSuffix))
    {
        return createTr("Extracted file %1 exceeds size limits").arg(
            message.mid(extractedFilePrefix.size(), message.size() -
                extractedFilePrefix.size() - extractedFileSuffix.size()));
    }
    QString const writePrefix = QStringLiteral("Could not write ");
    if (message.startsWith(writePrefix)) {
        return createTr("Could not write %1").arg(message.mid(writePrefix.size()));
    }
    QString const packageSizePrefix =
        QStringLiteral("Mascot package exceeds the maximum size of ");
    if (message.startsWith(packageSizePrefix) && message.endsWith(sizeSuffix)) {
        return createTr("Mascot package exceeds the maximum size of %1 bytes").arg(
            message.mid(packageSizePrefix.size(), message.size() -
                packageSizePrefix.size() - sizeSuffix.size()));
    }
    QString const invalidInfoPrefix = QStringLiteral(
        "info.json is invalid; fallback metadata will be generated (");
    if (message.startsWith(invalidInfoPrefix) && message.endsWith(QLatin1Char(')'))) {
        QString const reason = message.mid(
            invalidInfoPrefix.size(), message.size() - invalidInfoPrefix.size() - 1);
        return createTr(
            "info.json is invalid; fallback metadata will be generated (%1)")
            .arg(localizedMetadataReason(reason));
    }
    return message.isEmpty() ? createTr("Unknown error")
        : createTr("The archive contains invalid or unsupported content.");
}

QString localizedPackageMessageList(QString const& message)
{
    QStringList messages = message.split(QStringLiteral("; "), Qt::SkipEmptyParts);
    for (QString& item : messages) {
        item = localizedPackageMessage(item);
    }
    return messages.join(QLatin1Char('\n'));
}

void drawCreateFocusFrame(QWidget *widget)
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

class CreatePushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawCreateFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(CreatePushButton)
};

class CreateToolButton final : public ElaToolButton {
public:
    explicit CreateToolButton(QWidget *parent = nullptr):
        ElaToolButton(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaToolButton::paintEvent(event);
        drawCreateFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(CreateToolButton)
};

class ResponsiveCreatePathRow final : public QWidget {
public:
    ResponsiveCreatePathRow(QWidget *pathEdit, QWidget *button,
        QWidget *parent = nullptr):
        QWidget(parent),
        m_layout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(8);
        m_layout->addWidget(pathEdit, 1);
        m_layout->addWidget(button);
        applyLayoutMode();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        applyLayoutMode();
    }

private:
    void applyLayoutMode()
    {
        bool compact = width() > 0 && width() < kCompactCreateRowWidth;
        if (m_hasLayoutMode && compact == m_compact) {
            return;
        }
        m_hasLayoutMode = true;
        m_compact = compact;
        m_layout->setDirection(compact
            ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }

    QBoxLayout *m_layout;
    bool m_hasLayoutMode = false;
    bool m_compact = false;

    Q_DISABLE_COPY_MOVE(ResponsiveCreatePathRow)
};

void configureCreateButton(QWidget *button, QString const& accessibleName,
    QString const& accessibleDescription, int iconWidth = 0)
{
    button->setMinimumHeight(38);
    constexpr int horizontalPadding = 32;
    button->setMinimumWidth(button->fontMetrics().horizontalAdvance(
        accessibleName) + iconWidth + horizontalPadding);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(accessibleName);
    button->setAccessibleDescription(accessibleDescription);
    button->setToolTip(accessibleDescription);
}

void configurePrimaryButton(CreatePushButton *button, QString const& description)
{
    configureCreateButton(button, button->text(), description);
    button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
    button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
    button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
    button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
    button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
    button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
    button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
}

CreateToolButton *createCreateCommand(QWidget *parent, QString const& text,
    QString const& description, ElaIconType::IconName icon)
{
    auto *button = new CreateToolButton(parent);
    button->setText(text);
    button->setElaIcon(icon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIsTransparent(false);
    configureCreateButton(button, text, description, button->iconSize().width());
    return button;
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
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("CreatePanel"));
    return panel;
}

void addCreateStepHeader(QVBoxLayout *layout, QWidget *parent,
    QString const& step, QString const& title, QString const& description)
{
    auto *stepLabel = new QLabel(step, parent);
    stepLabel->setProperty("createStepLabel", true);
    layout->addWidget(stepLabel);

    auto *titleLabel = new QLabel(title, parent);
    titleLabel->setProperty("createStepTitle", true);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    layout->addWidget(makeCreateDescription(parent, description));
}

void applyCreateTheme(QWidget *createPage)
{
    auto mode = eTheme->getThemeMode();
    createPage->setStyleSheet(QString(
        "#CreatePanel {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel[muted=\"true\"] { color: %3; }"
        "QLabel[createStepLabel=\"true\"] {"
        "  color: %4;"
        "  font-weight: 600;"
        "}"
        "QLabel[createStepTitle=\"true\"] {"
        "  color: %5;"
        "  font-weight: 600;"
        "}"
        "QLabel[infoJsonError=\"true\"] {"
        "  color: %6;"
        "  font-weight: 600;"
        "}"
    ).arg(ElaThemeColor(mode, WindowBase).name(),
        ElaThemeColor(mode, BasicBorder).name(),
        ElaThemeColor(mode, BasicDetailsText).name(),
        ElaThemeColor(mode, PrimaryNormal).name(),
        ElaThemeColor(mode, BasicText).name(),
        ElaThemeColor(mode, StatusDanger).name()));
}

QString candidateSupportingDetails(LegacyMascotCandidate const& candidate)
{
    QStringList lines;
    for (auto const& warning : candidate.warnings) {
        if (warning.startsWith(QStringLiteral(
            "info.json is invalid; fallback metadata will be generated (")))
        {
            continue;
        }
        lines.append(localizedPackageMessage(warning));
    }
    for (auto const& error : candidate.errors) {
        lines.append(localizedPackageMessage(error));
    }
    return lines.join(QStringLiteral("\n"));
}

QString candidateStatusDetails(bool convertible, bool infoJsonValid,
    QString const& supportingDetails)
{
    QStringList lines;
    if (!convertible) {
        lines.append(createTr(
            "Cannot convert until the missing content is fixed."));
    }
    else if (!infoJsonValid) {
        lines.append(createTr(
            "Fix invalid info.json content before converting."));
    }
    else {
        lines.append(createTr("Ready to convert."));
    }
    if (!supportingDetails.isEmpty()) {
        lines.append(supportingDetails);
    }
    return lines.join(QStringLiteral("\n"));
}

QString candidateDetails(MascotMetadata const& metadata,
    QString const& statusDetails)
{
    QStringList lines;
    if (!metadata.version.isEmpty()) {
        lines.append(createTr("Version: %1").arg(metadata.version));
    }
    if (!metadata.author.isEmpty()) {
        lines.append(createTr("Author: %1").arg(metadata.author));
    }
    lines.append(statusDetails);
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
                    localizedPackageMessageList(result.errorMessage)));
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
    auto *pageLayout = new QVBoxLayout(m_ui->createPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(m_ui->createPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setAccessibleName(tr("Create Mascot Package"));
    pageLayout->addWidget(scrollArea);

    auto *content = new QWidget(scrollArea);
    auto *root = new QVBoxLayout(content);
    scrollArea->setWidget(content);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    root->addWidget(makeCreateTitle(content, tr("Create Mascot Package")));
    root->addWidget(makeCreateDescription(content,
        tr("Check a Shimeji zip archive, choose the mascots to convert, and write .mascot packages to a folder you choose.")));

    auto *inputPanel = makeCreatePanel(content);
    auto *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(14, 12, 14, 12);
    inputLayout->setSpacing(8);
    addCreateStepHeader(inputLayout, inputPanel, tr("Step 1"),
        tr("Choose a Shimeji archive"),
        tr("Select the .zip archive that contains the mascots you want to convert."));

    auto *zipLabel = new QLabel(tr("Shimeji archive"), inputPanel);
    inputLayout->addWidget(zipLabel);
    auto *zipEdit = new QLineEdit(inputPanel);
    zipEdit->setReadOnly(true);
    zipEdit->setPlaceholderText(tr("No archive selected"));
    zipEdit->setAccessibleName(tr("Shimeji archive path"));
    zipEdit->setAccessibleDescription(
        tr("Path to the selected Shimeji zip archive."));
    zipLabel->setBuddy(zipEdit);
    auto *chooseZipButton = createCreateCommand(inputPanel, tr("Choose Zip..."),
        tr("Choose a Shimeji zip archive to inspect."), ElaIconType::FileZipper);
    inputLayout->addWidget(new ResponsiveCreatePathRow(
        zipEdit, chooseZipButton, inputPanel));

    auto *checkButton = createCreateCommand(inputPanel, tr("Check Content"),
        tr("Inspect the selected archive and find convertible mascots."),
        ElaIconType::FileMagnifyingGlass);
    checkButton->setEnabled(false);
    inputLayout->addWidget(checkButton, 0, Qt::AlignLeft);

    root->addWidget(inputPanel);

    auto *candidatePanel = makeCreatePanel(content);
    auto *candidateLayout = new QVBoxLayout(candidatePanel);
    candidateLayout->setContentsMargins(14, 12, 14, 12);
    candidateLayout->setSpacing(8);
    addCreateStepHeader(candidateLayout, candidatePanel, tr("Step 2"),
        tr("Review mascots"),
        tr("Select mascots to convert, then review or edit each complete info.json file."));
    auto *statusLabel = makeCreateDescription(candidatePanel, tr("No archive checked yet."));
    statusLabel->setAccessibleName(tr("Archive check status"));
    candidateLayout->addWidget(statusLabel);
    auto *candidateLabel = new QLabel(tr("Mascots in archive"), candidatePanel);
    candidateLayout->addWidget(candidateLabel);
    auto *candidateList = new QListWidget(candidatePanel);
    candidateList->setMinimumHeight(150);
    candidateList->setSelectionMode(QListWidget::SingleSelection);
    candidateList->setEnabled(false);
    candidateList->setAccessibleName(tr("Mascots in archive"));
    candidateList->setAccessibleDescription(
        tr("Check the mascots that should be converted."));
    candidateLabel->setBuddy(candidateList);
    candidateLayout->addWidget(candidateList, 1);
    auto *infoJsonLabel = new QLabel(tr("info.json"), candidatePanel);
    candidateLayout->addWidget(infoJsonLabel);
    auto *infoJsonEditor = new QPlainTextEdit(candidatePanel);
    infoJsonEditor->setEnabled(false);
    infoJsonEditor->setMinimumHeight(180);
    infoJsonEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    infoJsonEditor->setTabChangesFocus(true);
    infoJsonEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    infoJsonEditor->setPlaceholderText(
        tr("Select a mascot to view and edit its complete info.json file."));
    infoJsonEditor->setAccessibleName(tr("Selected mascot info.json editor"));
    infoJsonEditor->setAccessibleDescription(
        tr("Complete editable JSON metadata for the currently selected mascot."));
    infoJsonLabel->setBuddy(infoJsonEditor);
    candidateLayout->addWidget(infoJsonEditor);
    auto *infoJsonStatus = makeCreateDescription(candidatePanel,
        tr("Select a mascot to edit its metadata."));
    infoJsonStatus->setAccessibleName(tr("info.json validation status"));
    candidateLayout->addWidget(infoJsonStatus);
    root->addWidget(candidatePanel, 1);

    auto applyInfoJsonStatus = [infoJsonStatus](QString const& text, bool error) {
        bool styleChanged = infoJsonStatus->property("infoJsonError").toBool() !=
            error;
        infoJsonStatus->setProperty("infoJsonError", error);
        infoJsonStatus->setProperty("muted", !error);
        infoJsonStatus->setText(text);
        if (styleChanged) {
            infoJsonStatus->style()->unpolish(infoJsonStatus);
            infoJsonStatus->style()->polish(infoJsonStatus);
        }
    };

    auto validateInfoJsonItem = [applyInfoJsonStatus](QListWidgetItem *item) {
        if (item == nullptr) {
            applyInfoJsonStatus(
                createTr("Select a mascot to edit its metadata."), false);
            return;
        }
        MascotMetadata metadata;
        QString error = infoJsonValidationError(
            item->data(kCandidateInfoJsonRole).toByteArray(), &metadata);
        bool valid = error.isEmpty();
        item->setData(kCandidateInfoJsonValidRole, valid);
        item->setData(kCandidateInfoJsonErrorRole, error);
        bool const convertible =
            item->flags().testFlag(Qt::ItemIsUserCheckable);
        if (valid) {
            item->setText(metadata.name);
            item->setToolTip(candidateDetails(metadata, candidateStatusDetails(
                convertible, true,
                item->data(kCandidateDetailsRole).toString())));
        }
        else {
            item->setToolTip(candidateStatusDetails(convertible, false,
                item->data(kCandidateDetailsRole).toString()));
        }
        applyInfoJsonStatus(valid ? createTr("Valid JSON.")
            : createTr("Invalid JSON: %1").arg(error), !valid);
    };

    auto *infoJsonValidationTimer = new QTimer(candidatePanel);
    infoJsonValidationTimer->setSingleShot(true);
    infoJsonValidationTimer->setInterval(250);

    auto flushInfoJsonItem = [candidateList, infoJsonEditor,
        validateInfoJsonItem](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        QByteArray const infoJson = infoJsonEditor->toPlainText().toUtf8();
        QSignalBlocker listBlocker(candidateList);
        item->setData(kCandidateInfoJsonRole, infoJson);
        item->setData(kCandidateInfoJsonPendingRole, false);
        validateInfoJsonItem(item);
    };

    connect(candidateList, &QListWidget::currentItemChanged, this,
        [candidateList, infoJsonEditor, infoJsonValidationTimer,
            flushInfoJsonItem, applyInfoJsonStatus](QListWidgetItem *current,
                QListWidgetItem *previous) {
            infoJsonValidationTimer->stop();
            if (previous != nullptr &&
                previous->data(kCandidateInfoJsonPendingRole).toBool())
            {
                flushInfoJsonItem(previous);
            }
            QSignalBlocker blocker(infoJsonEditor);
            if (current == nullptr) {
                infoJsonEditor->clear();
                infoJsonEditor->setEnabled(false);
                applyInfoJsonStatus(
                    createTr("Select a mascot to edit its metadata."), false);
                return;
            }
            infoJsonEditor->setPlainText(
                QString::fromUtf8(current->data(kCandidateInfoJsonRole).toByteArray()));
            infoJsonEditor->setEnabled(
                current->flags().testFlag(Qt::ItemIsUserCheckable));
            bool const valid =
                current->data(kCandidateInfoJsonValidRole).toBool();
            QString const error =
                current->data(kCandidateInfoJsonErrorRole).toString();
            applyInfoJsonStatus(valid ? createTr("Valid JSON.")
                : createTr("Invalid JSON: %1").arg(error), !valid);
        });

    auto *outputPanel = makeCreatePanel(content);
    auto *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(14, 12, 14, 12);
    outputLayout->setSpacing(8);
    addCreateStepHeader(outputLayout, outputPanel, tr("Step 3"),
        tr("Generate mascot packages"),
        tr("Choose where to save the converted .mascot packages, then generate them."));
    auto *outputLabel = new QLabel(tr("Output folder"), outputPanel);
    outputLayout->addWidget(outputLabel);
    auto *outputEdit = new QLineEdit(outputPanel);
    outputEdit->setReadOnly(true);
    outputEdit->setPlaceholderText(tr("No output folder selected"));
    outputEdit->setAccessibleName(tr("Output folder path"));
    outputEdit->setAccessibleDescription(
        tr("Folder where converted mascot packages will be saved."));
    outputLabel->setBuddy(outputEdit);
    auto *chooseOutputButton = createCreateCommand(outputPanel, tr("Choose Folder..."),
        tr("Choose the folder for converted mascot packages."), ElaIconType::FolderOpen);
    outputLayout->addWidget(new ResponsiveCreatePathRow(
        outputEdit, chooseOutputButton, outputPanel));

    auto *convertButton = new CreatePushButton(tr("Generate .mascot"), outputPanel);
    configurePrimaryButton(convertButton,
        tr("Generate packages for the checked mascots."));
    convertButton->setEnabled(false);
    outputLayout->addWidget(convertButton, 0, Qt::AlignLeft);

    auto *resultLabel = new QLabel(tr("Conversion results"), outputPanel);
    outputLayout->addWidget(resultLabel);
    auto *resultText = new QPlainTextEdit(outputPanel);
    resultText->setReadOnly(true);
    resultText->setMinimumHeight(90);
    resultText->setPlaceholderText(tr("Conversion results will appear here."));
    resultText->setAccessibleName(tr("Conversion results"));
    resultText->setAccessibleDescription(
        tr("Results from the most recent package conversion."));
    resultLabel->setBuddy(resultText);
    outputLayout->addWidget(resultText);
    root->addWidget(outputPanel);

    auto updateConvertAvailability = [zipEdit, outputEdit, candidateList,
        convertButton]() {
        bool hasCheckedCandidate = false;
        bool checkedInfoJsonIsValid = true;
        for (int i = 0; i < candidateList->count(); ++i) {
            auto *item = candidateList->item(i);
            if (item->flags().testFlag(Qt::ItemIsUserCheckable) &&
                item->checkState() == Qt::Checked) {
                hasCheckedCandidate = true;
                checkedInfoJsonIsValid = checkedInfoJsonIsValid &&
                    item->data(kCandidateInfoJsonValidRole).toBool();
            }
        }
        bool busy = convertButton->property("createBusy").toBool();
        convertButton->setEnabled(!busy && !zipEdit->text().trimmed().isEmpty() &&
            !outputEdit->text().trimmed().isEmpty() && hasCheckedCandidate &&
            checkedInfoJsonIsValid);
    };
    connect(candidateList, &QListWidget::itemChanged,
        m_ui->createPage, updateConvertAvailability);
    connect(candidateList, &QListWidget::currentItemChanged,
        m_ui->createPage, [updateConvertAvailability]() {
            updateConvertAvailability();
        });
    connect(infoJsonEditor, &QPlainTextEdit::textChanged, m_ui->createPage,
        [candidateList, infoJsonEditor, infoJsonValidationTimer,
            applyInfoJsonStatus, convertButton]() {
            auto *item = candidateList->currentItem();
            if (item == nullptr) {
                return;
            }
            {
                QSignalBlocker listBlocker(candidateList);
                item->setData(kCandidateInfoJsonDirtyRole, true);
                item->setData(kCandidateInfoJsonPendingRole, true);
                item->setData(kCandidateInfoJsonValidRole, false);
                item->setData(kCandidateInfoJsonErrorRole, QString());
            }
            applyInfoJsonStatus(createTr("Checking JSON..."), false);
            convertButton->setEnabled(false);
            infoJsonValidationTimer->start();
        });
    connect(infoJsonValidationTimer, &QTimer::timeout, m_ui->createPage,
        [candidateList, flushInfoJsonItem, updateConvertAvailability]() {
            auto *item = candidateList->currentItem();
            if (item == nullptr) {
                return;
            }
            flushInfoJsonItem(item);
            updateConvertAvailability();
        });

    auto setConversionInputsEnabled = [zipEdit, chooseZipButton, checkButton,
        candidateList, infoJsonEditor, chooseOutputButton](bool enabled) {
        chooseZipButton->setEnabled(enabled);
        checkButton->setEnabled(enabled && !zipEdit->text().trimmed().isEmpty());
        candidateList->setEnabled(enabled && candidateList->count() > 0);
        auto *currentItem = candidateList->currentItem();
        infoJsonEditor->setEnabled(enabled && currentItem != nullptr &&
            currentItem->flags().testFlag(Qt::ItemIsUserCheckable));
        chooseOutputButton->setEnabled(enabled);
    };

    connect(chooseZipButton, &ElaToolButton::clicked, this, [this, zipEdit,
        candidateList, statusLabel, infoJsonEditor, resultText, checkButton,
        updateConvertAvailability]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Choose Shimeji Zip Archive"),
            QString(), tr("Zip Archives (*.zip);;All Files (*)"));
        if (path.isEmpty()) {
            return;
        }
        zipEdit->setText(path);
        candidateList->clear();
        candidateList->setEnabled(false);
        infoJsonEditor->clear();
        infoJsonEditor->setEnabled(false);
        statusLabel->setText(tr("Archive selected. Run content check before generating."));
        resultText->clear();
        checkButton->setEnabled(true);
        updateConvertAvailability();
    });

    connect(chooseOutputButton, &ElaToolButton::clicked, this,
        [this, outputEdit, updateConvertAvailability]() {
        QString path = QFileDialog::getExistingDirectory(this, tr("Choose Output Folder"));
        if (!path.isEmpty()) {
            outputEdit->setText(path);
            updateConvertAvailability();
        }
    });

    connect(checkButton, &ElaToolButton::clicked, this,
        [this, zipEdit, candidateList, statusLabel, infoJsonEditor,
            checkButton, chooseZipButton, resultText, updateConvertAvailability]() {
            QString path = zipEdit->text().trimmed();
            if (path.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Choose a .zip archive first."));
                return;
            }
            checkButton->setEnabled(false);
            chooseZipButton->setEnabled(false);
            statusLabel->setText(tr("Checking archive content..."));
            candidateList->clear();
            candidateList->setEnabled(false);
            infoJsonEditor->clear();
            infoJsonEditor->setEnabled(false);
            resultText->clear();
            updateConvertAvailability();

            auto analyzeArchiveTask = [path]() {
                return MascotPackage::analyzeLegacyArchive(path);
            };
            auto applyArchiveAnalysis =
                [this, candidateList, statusLabel,
                checkButton, chooseZipButton,
                updateConvertAvailability](LegacyArchiveAnalysis analysis) {
                const auto& candidates = analysis.candidates;
                checkButton->setEnabled(true);
                chooseZipButton->setEnabled(true);
                candidateList->clear();
                if (candidates.isEmpty()) {
                    statusLabel->setText(analysis.errorMessage.isEmpty()
                        ? tr("No Shimeji mascots were found in the archive.")
                        : localizedPackageMessage(analysis.errorMessage));
                    updateConvertAvailability();
                    return;
                }

                int convertibleCount = 0;
                for (auto const& candidate : candidates) {
                    bool const ready =
                        candidate.convertible && candidate.infoJsonValid;
                    if (ready) {
                        ++convertibleCount;
                    }
                    auto *item = new QListWidgetItem(candidate.metadata.name);
                    item->setData(kCandidateNameRole,
                        candidate.sourceName.isEmpty()
                            ? candidate.name : candidate.sourceName);
                    item->setData(kCandidateInfoJsonRole, candidate.infoJson);
                    item->setData(kCandidateInfoJsonValidRole,
                        candidate.infoJsonValid);
                    item->setData(kCandidateInfoJsonDirtyRole, false);
                    item->setData(kCandidateInfoJsonPendingRole, false);
                    QString const validationError = candidate.infoJsonValid
                        ? QString()
                        : localizedInfoJsonValidationError(candidate.infoJsonError);
                    item->setData(kCandidateInfoJsonErrorRole, validationError);
                    QString const supportingDetails =
                        candidateSupportingDetails(candidate);
                    item->setData(kCandidateDetailsRole, supportingDetails);
                    item->setToolTip(candidateDetails(candidate.metadata,
                        candidateStatusDetails(candidate.convertible,
                            candidate.infoJsonValid, supportingDetails)));
                    if (candidate.convertible) {
                        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                        item->setCheckState(ready ? Qt::Checked : Qt::Unchecked);
                    }
                    else {
                        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
                    }
                    candidateList->addItem(item);
                }
                candidateList->setEnabled(true);

                if (candidateList->count() > 0) {
                    candidateList->setCurrentRow(0);
                }

                QString status =
                    tr("Found %n mascot(s).", nullptr, candidates.size()) +
                    QLatin1Char(' ') +
                    tr("%n ready to convert.", nullptr, convertibleCount);
                if (!analysis.ok && !analysis.errorMessage.isEmpty()) {
                    status.prepend(localizedPackageMessage(analysis.errorMessage) +
                        QLatin1Char('\n'));
                }
                statusLabel->setText(status);
                updateConvertAvailability();
            };
            QtConcurrent::run(analyzeArchiveTask).then(
                m_ui->createPage, applyArchiveAnalysis);
        });

    connect(convertButton, &ElaPushButton::clicked, this,
        [this, zipEdit, outputEdit, candidateList, convertButton, resultText,
            updateConvertAvailability, setConversionInputsEnabled]() {
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
            QHash<QString, QByteArray> infoJsonOverrides;
            for (int i = 0; i < candidateList->count(); ++i) {
                auto *item = candidateList->item(i);
                if (item->flags().testFlag(Qt::ItemIsUserCheckable) &&
                    item->checkState() == Qt::Checked)
                {
                    if (!item->data(kCandidateInfoJsonValidRole).toBool()) {
                        QMessageBox::warning(this, tr("Create"),
                            tr("Fix invalid info.json content before generating."));
                        return;
                    }
                    QString name = item->data(kCandidateNameRole).toString();
                    selectedNames.append(name);
                    if (item->data(kCandidateInfoJsonDirtyRole).toBool()) {
                        infoJsonOverrides.insert(name,
                            item->data(kCandidateInfoJsonRole).toByteArray());
                    }
                }
            }
            if (selectedNames.isEmpty()) {
                QMessageBox::warning(this, tr("Create"), tr("Select at least one mascot to convert."));
                return;
            }

            convertButton->setProperty("createBusy", true);
            setConversionInputsEnabled(false);
            updateConvertAvailability();
            resultText->setPlainText(tr("Generating .mascot package(s)..."));
            auto writePackagesTask = [archivePath, outputPath, selectedNames,
                infoJsonOverrides]() {
                return MascotPackage::writeLegacyArchiveSelectionAsPackages(
                    archivePath, outputPath, selectedNames, infoJsonOverrides);
            };
            auto applyConversionResults = [convertButton, resultText,
                updateConvertAvailability,
                setConversionInputsEnabled](QList<LegacyMascotConversionResult> results) {
                convertButton->setProperty("createBusy", false);
                resultText->setPlainText(resultSummary(results));
                setConversionInputsEnabled(true);
                updateConvertAvailability();
            };
            QtConcurrent::run(writePackagesTask).then(
                m_ui->createPage, applyConversionResults);
        });

    setTabOrder(zipEdit, chooseZipButton);
    setTabOrder(chooseZipButton, checkButton);
    setTabOrder(checkButton, candidateList);
    setTabOrder(candidateList, infoJsonEditor);
    setTabOrder(infoJsonEditor, outputEdit);
    setTabOrder(outputEdit, chooseOutputButton);
    setTabOrder(chooseOutputButton, convertButton);
    setTabOrder(convertButton, resultText);

    applyCreateTheme(m_ui->createPage);
    connect(eTheme, &ElaTheme::themeModeChanged, m_ui->createPage, [this]() {
        applyCreateTheme(m_ui->createPage);
    });

    addPageNode(tr("Create"), m_ui->createPage, ElaIconType::WandMagicSparkles);
}
