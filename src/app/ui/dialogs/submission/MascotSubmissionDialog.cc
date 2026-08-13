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

#include "MascotSubmissionDialog.hpp"

#include "shijima-qt/GitHubAuthManager.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/MascotSubmissionClient.hpp"
#include "shijima-qt/Secrets.hpp"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QSizePolicy>
#include <QStringList>
#include <QStyleOptionFocusRect>
#include <QVBoxLayout>

#include <QUuid>

#include "ElaCheckBox.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaTheme.h"

namespace {

QString submissionTr(char const *source)
{
    return QCoreApplication::translate("MascotSubmissionDialog", source);
}

QString withSubmissionDetail(QString message, QString const& detail)
{
    if (!detail.trimmed().isEmpty()) {
        message += QLatin1Char('\n') + submissionTr("Submission details: %1")
            .arg(redactSensitiveText(detail));
    }
    return message;
}

QString localizedSubmissionError(QString const& code, QString const& detail)
{
    char const *source = "Submission failed.";
    if (code == QStringLiteral("submission.file_missing")) {
        source = "The selected mascot package does not exist.";
    }
    else if (code == QStringLiteral("submission.not_configured")) {
        source = "The submission service is not configured by the maintainer.";
    }
    else if (code == QStringLiteral("submission.not_signed_in")) {
        source = "Sign in with GitHub before submitting a mascot.";
    }
    else if (code == QStringLiteral("submission.auth_failed")) {
        source = "The submission service could not authenticate GitHub.";
    }
    else if (code == QStringLiteral("submission.auth_invalid")) {
        source = "The submission service returned no session token.";
    }
    else if (code == QStringLiteral("submission.file_unreadable")) {
        source = "The selected mascot package could not be read.";
    }
    else if (code == QStringLiteral("submission.canceled")) {
        source = "The upload was canceled.";
    }
    else if (code == QStringLiteral("submission.service_error")) {
        source = "The submission service rejected the upload.";
    }
    else if (code == QStringLiteral("submission.network_error")) {
        source = "Could not reach the submission service.";
    }
    return withSubmissionDetail(submissionTr(source), detail);
}

QString localizedValidationError(QString const& error)
{
    if (error == QStringLiteral("Mascot package does not exist")) {
        return submissionTr("Mascot package does not exist.");
    }
    if (error == QStringLiteral("Package is not a valid ZIP archive")) {
        return submissionTr("Package is not a valid ZIP archive.");
    }
    if (error.startsWith(QStringLiteral("Package is missing "))) {
        return submissionTr("Package is missing %1").arg(
            error.mid(QStringLiteral("Package is missing ").size()));
    }
    if (error == QStringLiteral(
            "Package must contain actions.xml, behaviors.xml, and img/*.png"))
    {
        return submissionTr(
            "Package must contain actions.xml, behaviors.xml, and img/*.png.");
    }
    if (error.startsWith(QStringLiteral("Could not read "))) {
        return submissionTr("Could not read %1").arg(
            error.mid(QStringLiteral("Could not read ").size()));
    }
    if (error.startsWith(QStringLiteral("Missing "))) {
        return submissionTr("Missing %1").arg(
            error.mid(QStringLiteral("Missing ").size()));
    }
    if (error.startsWith(QStringLiteral("Unsupported or unsafe package entry: "))) {
        return submissionTr("Unsupported or unsafe package entry: %1").arg(
            error.mid(QStringLiteral("Unsupported or unsafe package entry: ").size()));
    }
    if (error.startsWith(QStringLiteral("Package contains a forbidden payload entry: "))) {
        return submissionTr("Package contains a forbidden payload entry: %1").arg(
            error.mid(QStringLiteral("Package contains a forbidden payload entry: ").size()));
    }
    if (error.startsWith(QStringLiteral("Package entry ")) &&
        error.endsWith(QStringLiteral(" exceeds size limits"))) {
        return submissionTr("Package entry %1 exceeds size limits").arg(
            error.mid(QStringLiteral("Package entry ").size(),
                error.size() - QStringLiteral("Package entry ").size() -
                QStringLiteral(" exceeds size limits").size()));
    }
    if (error.startsWith(QStringLiteral("Package must contain "))) {
        return submissionTr("Package must contain %1").arg(
            error.mid(QStringLiteral("Package must contain ").size()));
    }
    if (error.startsWith(QStringLiteral("Image ")) &&
        error.endsWith(QStringLiteral(" is not a valid PNG"))) {
        return submissionTr("Image %1 is not a valid PNG").arg(
            error.mid(QStringLiteral("Image ").size(),
                error.size() - QStringLiteral("Image ").size() -
                QStringLiteral(" is not a valid PNG").size()));
    }
    static QRegularExpression const imageBudgetPattern(
        QStringLiteral("^Image (.+) exceeds the maximum pixel count of ([0-9]+)$"));
    QRegularExpressionMatch imageBudgetMatch = imageBudgetPattern.match(error);
    if (imageBudgetMatch.hasMatch()) {
        return submissionTr("Image %1 exceeds the maximum pixel count of %2")
            .arg(imageBudgetMatch.captured(1), imageBudgetMatch.captured(2));
    }
    if (error == QStringLiteral("Package extracted data is too large")) {
        return submissionTr("Package extracted data is too large.");
    }
    if (error.startsWith(QStringLiteral(
            "Package image data exceeds the total pixel budget of "))) {
        return submissionTr(
            "Package image data exceeds the total pixel budget of %1")
            .arg(error.mid(QStringLiteral(
                "Package image data exceeds the total pixel budget of ").size()));
    }
    if (error == QStringLiteral("Could not create temporary extraction directory")) {
        return submissionTr("Could not create temporary extraction directory.");
    }
    if (error == QStringLiteral("Package does not contain any supported files")) {
        return submissionTr("Package does not contain any supported files.");
    }
    if (error == QStringLiteral("Archive contains symbolic links")) {
        return submissionTr("Archive contains symbolic links.");
    }
    if (error == QStringLiteral("Archive extracted an unsafe path")) {
        return submissionTr("Archive extracted an unsafe path.");
    }
    return error;
}

void drawSubmissionFocusFrame(QWidget *widget)
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

class SubmissionPushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawSubmissionFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(SubmissionPushButton)
};

constexpr int kCompactSubmissionPathRowWidth = 520;

QRect submissionAvailableGeometry(QWidget *widget)
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

void sizeSubmissionDialog(QDialog *dialog, QLayout *layout)
{
    layout->activate();
    QRect available = submissionAvailableGeometry(dialog);
    int availableWidth = qMax(1, available.width() - 32);
    int availableHeight = qMax(1, available.height() - 32);
    int maxWidth = qMin(720, availableWidth);
    int maxHeight = qMin(760, availableHeight);
    int minWidth = qMin(520, maxWidth);
    int minHeight = qMin(420, maxHeight);
    QSize hint = layout->sizeHint();
    QSize preferred(
        qBound(minWidth, qMax(minWidth, hint.width()), maxWidth),
        qBound(minHeight, qMax(minHeight, hint.height()), maxHeight));
    dialog->setMinimumSize(minWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    dialog->resize(preferred);
}

class ResponsiveSubmissionPathRow final : public QWidget {
public:
    ResponsiveSubmissionPathRow(QWidget *pathEdit, QWidget *button,
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
        bool compact = width() > 0 && width() < kCompactSubmissionPathRowWidth;
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

    Q_DISABLE_COPY_MOVE(ResponsiveSubmissionPathRow)
};

QFrame *makeSubmissionCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("submissionCard"));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    return card;
}

ElaText *makeSubmissionSectionTitle(QWidget *parent, QString const& text)
{
    auto *title = new ElaText(text, parent);
    title->setObjectName(QStringLiteral("submissionSectionTitle"));
    title->setTextPixelSize(17);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral(
        "#submissionSectionTitle { background: transparent; border: none; }"));
    return title;
}

QLabel *makeSubmissionDescription(QWidget *parent, QString const& text)
{
    auto *description = new QLabel(text, parent);
    description->setObjectName(QStringLiteral("submissionDescription"));
    description->setWordWrap(true);
    description->setTextFormat(Qt::PlainText);
    return description;
}

void configureSubmissionButton(QPushButton *button, bool primary = false)
{
    button->setMinimumHeight(38);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(button->text());
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAutoDefault(false);
    button->setDefault(false);
    if (primary) {
        auto *elaButton = qobject_cast<ElaPushButton *>(button);
        if (elaButton != nullptr) {
            elaButton->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
            elaButton->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
            elaButton->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
            elaButton->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
            elaButton->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
            elaButton->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
            elaButton->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
            elaButton->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
        }
    }
}

void applySubmissionTheme(QWidget *dialog)
{
    auto mode = eTheme->getThemeMode();
    QColor surface = ElaThemeColor(mode, DialogBase);
    QColor card = ElaThemeColor(mode, BasicBase);
    QColor field = ElaThemeColor(mode, DialogLayoutArea);
    QColor disabledSurface = ElaThemeColor(mode, BasicDisable);
    QColor border = ElaThemeColor(mode, BasicBorder);
    QColor text = ElaThemeColor(mode, BasicText);
    QColor muted = ElaThemeColor(mode, BasicDetailsText);
    QColor disabledText = ElaThemeColor(mode, BasicTextDisable);
    QColor accent = ElaThemeColor(mode, PrimaryNormal);

    dialog->setStyleSheet(QString(
        "#MascotSubmissionDialog { background-color: %1; color: %6; }"
        "#submissionCard { background-color: %2; color: %6;"
        " border: 1px solid %5; border-radius: 10px; }"
        "#submissionCard QLabel { color: %6; background: transparent; }"
        "#submissionDescription, #submissionFieldHelp { color: %7; }"
        "#submissionScrollArea, #submissionScrollArea > QWidget > QWidget,"
        "#submissionContent { background: transparent; border: none; }"
        "#submissionFieldLabel { color: %6; background: transparent; }"
        "#submissionStatus { color: %6; background: transparent; }"
        "#submissionLink { color: %9; background: transparent; }"
        "#submissionCard QLineEdit, #submissionCard QPlainTextEdit,"
        "#submissionCard ElaLineEdit, #submissionCard ElaPlainTextEdit {"
        " background-color: %3; color: %6; border: 1px solid %5;"
        " border-radius: 7px; padding: 6px 8px; }"
        "#submissionCard QLineEdit:focus, #submissionCard QPlainTextEdit:focus,"
        "#submissionCard ElaLineEdit:focus, #submissionCard ElaPlainTextEdit:focus {"
        " border: 1px solid %9; }"
        "#submissionCard QLineEdit:disabled, #submissionCard QPlainTextEdit:disabled,"
        "#submissionCard ElaLineEdit:disabled, #submissionCard ElaPlainTextEdit:disabled {"
        " background-color: %4; color: %8; }"
        "#submissionCard ElaCheckBox, #submissionCard QCheckBox {"
        " color: %6; spacing: 8px; }"
        "#submissionCard ElaCheckBox:focus, #submissionCard QCheckBox:focus {"
        " border-radius: 4px; padding: 2px; }"
    ).arg(surface.name(QColor::HexArgb), card.name(QColor::HexArgb),
        field.name(QColor::HexArgb), disabledSurface.name(QColor::HexArgb),
        border.name(QColor::HexArgb), text.name(QColor::HexArgb),
        muted.name(QColor::HexArgb), disabledText.name(QColor::HexArgb),
        accent.name(QColor::HexArgb)));

    auto applyPalette = [field, disabledSurface, text, muted, disabledText,
        accent](QWidget *widget) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Base, field);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, text);
        palette.setColor(QPalette::Disabled, QPalette::Base, disabledSurface);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabledText);
        widget->setPalette(palette);
    };
    for (auto *widget : dialog->findChildren<QLineEdit *>()) {
        applyPalette(widget);
    }
    for (auto *widget : dialog->findChildren<QPlainTextEdit *>()) {
        applyPalette(widget);
    }
}

}

MascotSubmissionDialog::MascotSubmissionDialog(GitHubAuthManager *auth,
    MascotSubmissionClient *client, QWidget *parent):
    QDialog(parent),
    m_auth(auth),
    m_client(client)
{
    setObjectName(QStringLiteral("MascotSubmissionDialog"));
    setWindowTitle(tr("Submit a Mascot"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *title = new ElaText(tr("Submit a Mascot"), this);
    title->setTextPixelSize(22);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral(
        "#ElaText { background: transparent; border: none; }"));
    layout->addWidget(title);
    layout->addWidget(makeSubmissionDescription(this,
        tr("Share a validated .mascot package with the community registry.")));

    auto *scrollArea = new ElaScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("submissionScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFocusPolicy(Qt::NoFocus);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("submissionContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 2, 0, 0);
    contentLayout->setSpacing(10);
    scrollArea->setWidget(content);
    layout->addWidget(scrollArea, 1);

    auto *packageCard = makeSubmissionCard(content);
    auto *packageLayout = new QVBoxLayout(packageCard);
    packageLayout->setContentsMargins(14, 12, 14, 12);
    packageLayout->setSpacing(6);
    packageLayout->addWidget(makeSubmissionSectionTitle(packageCard, tr("Package")));
    packageLayout->addWidget(makeSubmissionDescription(packageCard,
        tr("Choose the validated .mascot package to upload.")));

    m_packagePath = new ElaLineEdit(packageCard);
    m_packagePath->setAccessibleName(tr("Mascot package path"));
    m_packagePath->setAccessibleDescription(
        tr("Path to the .mascot package to submit."));
    m_packagePath->setPlaceholderText(tr("No package selected"));
    m_pickButton = new SubmissionPushButton(tr("Choose .mascot..."), packageCard);
    configureSubmissionButton(m_pickButton);
    m_pickButton->setAccessibleDescription(
        tr("Select a .mascot package to submit."));
    auto *pathRow = new ResponsiveSubmissionPathRow(
        m_packagePath, m_pickButton, packageCard);
    auto *pathLabel = new QLabel(tr("Mascot package"), packageCard);
    pathLabel->setBuddy(m_packagePath);
    pathLabel->setObjectName(QStringLiteral("submissionFieldLabel"));
    packageLayout->addWidget(pathLabel);
    packageLayout->addWidget(pathRow);
    connect(m_pickButton, &QPushButton::clicked, this,
        &MascotSubmissionDialog::pickPackage);
    contentLayout->addWidget(packageCard);

    auto *metadataCard = makeSubmissionCard(content);
    auto *metadataLayout = new QVBoxLayout(metadataCard);
    metadataLayout->setContentsMargins(14, 12, 14, 12);
    metadataLayout->setSpacing(6);
    metadataLayout->addWidget(makeSubmissionSectionTitle(metadataCard, tr("Metadata")));
    metadataLayout->addWidget(makeSubmissionDescription(metadataCard,
        tr("Add the public information that will appear in the registry.")));
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 2, 0, 0);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto addField = [form, metadataCard](QString const& labelText,
        QWidget *widget) {
        auto *label = new QLabel(labelText, metadataCard);
        label->setBuddy(widget);
        label->setObjectName(QStringLiteral("submissionFieldLabel"));
        widget->setAccessibleName(labelText);
        form->addRow(label, widget);
    };

    m_id = new ElaLineEdit(metadataCard);
    m_id->setPlaceholderText(tr("lowercase-id (first publication is permanent)"));
    addField(tr("ID"), m_id);
    m_name = new ElaLineEdit(metadataCard);
    addField(tr("Name"), m_name);
    m_version = new ElaLineEdit(metadataCard);
    m_version->setPlaceholderText(tr("1.0.0"));
    addField(tr("Version"), m_version);
    m_summary = new ElaLineEdit(metadataCard);
    addField(tr("Summary"), m_summary);
    m_description = new ElaPlainTextEdit(metadataCard);
    m_description->setPlaceholderText(tr("Detailed description"));
    m_description->setMinimumHeight(96);
    m_description->setMaximumHeight(156);
    m_description->setTabChangesFocus(true);
    addField(tr("Description"), m_description);
    m_license = new ElaLineEdit(metadataCard);
    m_license->setPlaceholderText(tr("MIT"));
    addField(tr("License (SPDX)"), m_license);
    m_maintainers = new ElaLineEdit(metadataCard);
    m_maintainers->setPlaceholderText(tr("github logins, comma separated"));
    addField(tr("Maintainers"), m_maintainers);
    metadataLayout->addLayout(form);
    contentLayout->addWidget(metadataCard);

    auto *rightsCard = makeSubmissionCard(content);
    auto *rightsLayout = new QVBoxLayout(rightsCard);
    rightsLayout->setContentsMargins(14, 12, 14, 12);
    rightsLayout->setSpacing(6);
    rightsLayout->addWidget(makeSubmissionSectionTitle(rightsCard,
        tr("Rights and authorship")));
    rightsLayout->addWidget(makeSubmissionDescription(rightsCard,
        tr("Confirm that you are allowed to publish this work under the declared license.")));
    m_rightsConfirmed = new ElaCheckBox(
        tr("I confirm I have the right to publish this work under the "
           "declared license."), rightsCard);
    m_rightsConfirmed->setAccessibleName(m_rightsConfirmed->text());
    m_rightsConfirmed->setFocusPolicy(Qt::StrongFocus);
    m_rightsConfirmed->setMinimumHeight(28);
    rightsLayout->addWidget(m_rightsConfirmed);
    contentLayout->addWidget(rightsCard);

    auto *statusCard = makeSubmissionCard(content);
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(4);
    statusLayout->addWidget(makeSubmissionSectionTitle(statusCard, tr("Status")));
    m_statusLabel = new QLabel(statusCard);
    m_statusLabel->setObjectName(QStringLiteral("submissionStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setText(tr("Ready to submit."));
    statusLayout->addWidget(m_statusLabel);
    m_prLink = new QLabel(statusCard);
    m_prLink->setObjectName(QStringLiteral("submissionLink"));
    m_prLink->setOpenExternalLinks(true);
    m_prLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_prLink->setVisible(false);
    statusLayout->addWidget(m_prLink);
    contentLayout->addWidget(statusCard);

    m_submitButton = new SubmissionPushButton(tr("Submit"), this);
    auto *closeButton = new SubmissionPushButton(tr("Close"), this);
    configureSubmissionButton(m_submitButton, true);
    configureSubmissionButton(closeButton);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    buttonRow->addWidget(m_submitButton);
    layout->addLayout(buttonRow);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_submitButton, &QPushButton::clicked, this,
        &MascotSubmissionDialog::submit);
    connect(m_client, &MascotSubmissionClient::submissionFinished, this,
        [this](MascotSubmissionClient::SubmissionResult result) {
            setBusy(false);
            if (result.ok) {
                m_statusLabel->setText(tr("Submitted. Review PR %1.")
                    .arg(result.prNumber));
                m_prLink->setText(
                    QStringLiteral("<a href=\"%1\">%2</a>")
                        .arg(result.prUrl.toString().toHtmlEscaped(),
                             result.prUrl.toDisplayString().toHtmlEscaped()));
                m_prLink->setVisible(true);
            }
            else {
                m_prLink->clear();
                m_prLink->setVisible(false);
                m_statusLabel->setText(tr("Submission failed (%1): %2")
                    .arg(result.errorCode,
                         localizedSubmissionError(result.errorCode,
                             result.error)));
            }
        });
    connect(m_client, &MascotSubmissionClient::uploadProgress, this,
        [this](qint64 sent, qint64 total) {
            if (total > 0) {
                m_prLink->clear();
                m_prLink->setVisible(false);
                m_statusLabel->setText(tr("Uploading... %1 / %2")
                    .arg(QLocale().formattedDataSize(sent),
                         QLocale().formattedDataSize(total)));
        }
    });

    setTabOrder(m_packagePath, m_pickButton);
    setTabOrder(m_pickButton, m_id);
    setTabOrder(m_id, m_name);
    setTabOrder(m_name, m_version);
    setTabOrder(m_version, m_summary);
    setTabOrder(m_summary, m_description);
    setTabOrder(m_description, m_license);
    setTabOrder(m_license, m_maintainers);
    setTabOrder(m_maintainers, m_rightsConfirmed);
    setTabOrder(m_rightsConfirmed, closeButton);
    setTabOrder(closeButton, m_submitButton);

    applySubmissionTheme(this);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() {
        applySubmissionTheme(this);
    });
    sizeSubmissionDialog(this, layout);
    m_packagePath->setFocus();
}

void MascotSubmissionDialog::pickPackage() {
    QString path = QFileDialog::getOpenFileName(this,
        tr("Choose a .mascot package"), {},
        tr("NeurolingsCE packages (*.mascot)"));
    if (!path.isEmpty()) {
        m_packagePath->setText(path);
    }
}

void MascotSubmissionDialog::submit() {
    m_prLink->clear();
    m_prLink->setVisible(false);
    MascotPackageReport report;
    if (!MascotPackage::validatePackage(m_packagePath->text(), report)) {
        QStringList localizedErrors;
        for (QString const& error : report.errors) {
            localizedErrors.append(localizedValidationError(error));
        }
        m_statusLabel->setText(tr("Local validation failed:\n%1")
            .arg(localizedErrors.join(QStringLiteral("\n"))));
        return;
    }
    if (!m_rightsConfirmed->isChecked()) {
        m_statusLabel->setText(tr("Confirm your publication rights first."));
        return;
    }
    QStringList maintainers = m_maintainers->text().split(
        QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    QJsonObject metadata;
    metadata[QStringLiteral("id")] = m_id->text().trimmed();
    metadata[QStringLiteral("name")] = m_name->text().trimmed();
    metadata[QStringLiteral("version")] = m_version->text().trimmed();
    metadata[QStringLiteral("summary")] = m_summary->text().trimmed();
    metadata[QStringLiteral("description")] = m_description->toPlainText();
    metadata[QStringLiteral("license")] = m_license->text().trimmed();
    metadata[QStringLiteral("maintainers")] = QJsonArray::fromStringList(
        maintainers);
    metadata[QStringLiteral("authors")] = QJsonArray {
        QJsonObject {
            { QStringLiteral("githubLogin"),
                m_auth->userInfo().login },
            { QStringLiteral("githubUserId"),
                m_auth->userInfo().userId },
            { QStringLiteral("displayName"),
                m_auth->userInfo().displayName.isEmpty()
                    ? m_auth->userInfo().login
                    : m_auth->userInfo().displayName },
        },
    };
    metadata[QStringLiteral("isDerivative")] = false;
    metadata[QStringLiteral("minimumNeurolingsCEVersion")] =
        QStringLiteral(NEUROLINGSCE_VERSION);
    QByteArray metadataJson = QJsonDocument(metadata).toJson(
        QJsonDocument::Compact);
    QString idempotencyKey = QUuid::createUuid().toString(
        QUuid::Id128);
    setBusy(true);
    m_client->setAccessToken(m_auth->accessToken());
    m_client->submit(m_packagePath->text(), metadataJson, idempotencyKey);
}

void MascotSubmissionDialog::setBusy(bool busy) {
    m_submitButton->setEnabled(!busy);
    m_packagePath->setEnabled(!busy);
    m_pickButton->setEnabled(!busy);
    m_id->setEnabled(!busy);
    m_name->setEnabled(!busy);
    m_version->setEnabled(!busy);
    m_summary->setEnabled(!busy);
    m_description->setEnabled(!busy);
    m_license->setEnabled(!busy);
    m_maintainers->setEnabled(!busy);
    m_rightsConfirmed->setEnabled(!busy);
}
