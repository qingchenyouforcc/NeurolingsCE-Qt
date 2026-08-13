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
#include "shijima-qt/ShijimaHttpApi.hpp"
#include "shijima-qt/ShijimaLocalApi.hpp"
#include "../ManagerUiState.hpp"
#include "../ManagerUiHelpers.hpp"
#include "../../core/update/GitHubUpdateManager.hpp"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyleOptionFocusRect>
#include <QUrl>
#include <QVBoxLayout>

#include "shijima-qt/ui/dialogs/licenses/ShijimaLicensesDialog.hpp"
#include "ElaFlowLayout.h"
#include "ElaIcon.h"
#include "ElaPushButton.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaTheme.h"

#ifndef NEUROLINGSCE_VERSION
#define NEUROLINGSCE_VERSION "0.1.0"
#endif

namespace {

struct AboutColors {
    QColor page;
    QColor viewport;
    QColor content;
    QColor card;
    QColor border;
    QColor text;
    QColor secondary;
    QColor link;
    QColor disabled;
    QColor disabledSurface;
    QColor hover;
    QColor focus;
    QColor scrollHandle;
};

AboutColors themedColors()
{
    auto mode = eTheme->getThemeMode();
    return AboutColors {
        ElaThemeColor(mode, WindowBase),
        ElaThemeColor(mode, WindowCentralStackBase),
        ElaThemeColor(mode, BasicBase),
        ElaThemeColor(mode, BasicBase),
        ElaThemeColor(mode, BasicBorder),
        ElaThemeColor(mode, BasicText),
        ElaThemeColor(mode, BasicDetailsText),
        ElaThemeColor(mode, PrimaryNormal),
        ElaThemeColor(mode, BasicTextDisable),
        ElaThemeColor(mode, BasicDisable),
        ElaThemeColor(mode, BasicHover),
        ElaThemeColor(mode, PrimaryNormal),
        ElaThemeColor(mode, ScrollBarHandle),
    };
}

QString colorName(QColor const& color)
{
    return color.name(QColor::HexArgb);
}

QString linkHtml(QString const& text, QString const& url, QColor const& color)
{
    return QStringLiteral("<a href=\"%1\" style=\"color: %2;\">%3</a>")
        .arg(url, colorName(color), text.toHtmlEscaped());
}

void drawAboutFocusFrame(QWidget *widget)
{
    if (widget == nullptr || !widget->hasFocus()) {
        return;
    }
    QStyleOptionFocusRect option;
    option.initFrom(widget);
    option.rect = widget->rect().adjusted(3, 3, -3, -3);
    QPainter painter(widget);
    widget->style()->drawPrimitive(
        QStyle::PE_FrameFocusRect, &option, &painter, widget);
}

class AboutPushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawAboutFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(AboutPushButton)
};

class AboutContentWidget final : public QWidget {
public:
    explicit AboutContentWidget(QWidget *parent = nullptr):
        QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    void setContainer(QWidget *container, int horizontalMargins, int maximumWidth)
    {
        m_container = container;
        m_horizontalMargins = qMax(0, horizontalMargins);
        m_maximumWidth = qMax(0, maximumWidth);
        updateContainerWidth();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        updateContainerWidth();
    }

private:
    void updateContainerWidth()
    {
        if (m_container == nullptr) {
            return;
        }
        if (width() <= 0) {
            m_container->setMinimumWidth(0);
            m_container->setMaximumWidth(m_maximumWidth);
            return;
        }
        int availableWidth = qMax(0, width() - m_horizontalMargins);
        m_container->setFixedWidth(qMin(m_maximumWidth, availableWidth));
    }

    QWidget *m_container = nullptr;
    int m_horizontalMargins = 0;
    int m_maximumWidth = 0;

    Q_DISABLE_COPY_MOVE(AboutContentWidget)
};

class AboutCardHeader final : public ElaPushButton {
public:
    AboutCardHeader(QString const& title, QWidget *parent = nullptr):
        ElaPushButton(title, parent),
        m_title(title)
    {
        setObjectName(QStringLiteral("aboutCardHeader"));
        setCheckable(true);
        setAutoDefault(false);
        setDefault(false);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumHeight(46);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(title);
        setAccessibleDescription(
            QCoreApplication::translate("ShijimaManager", "Expand or collapse %1")
                .arg(title));
        setLayoutDirection(Qt::RightToLeft);
        connect(this, &QPushButton::clicked, this, [this](bool checked) {
            setExpanded(checked);
        });
    }

    void setBody(QWidget *body)
    {
        m_body = body;
        setExpanded(m_expanded);
    }

    void setExpanded(bool expanded)
    {
        m_expanded = expanded;
        setChecked(expanded);
        if (m_body != nullptr) {
            m_body->setVisible(expanded);
        }
        setAccessibleDescription(
            QCoreApplication::translate("ShijimaManager", "Expand or collapse %1")
                .arg(m_title));
        updateIcon();
    }

    void updateIcon()
    {
        QColor iconColor = ElaThemeColor(eTheme->getThemeMode(), BasicText);
        setIcon(ElaIcon::getInstance()->getElaIcon(
            m_expanded ? ElaIconType::ChevronDown : ElaIconType::ChevronRight,
            16, iconColor));
        setIconSize(QSize(16, 16));
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawAboutFocusFrame(this);
    }

private:
    QString m_title;
    QWidget *m_body = nullptr;
    bool m_expanded = true;

    Q_DISABLE_COPY_MOVE(AboutCardHeader)
};

class ResponsiveAboutButtonRow final : public QWidget {
public:
    explicit ResponsiveAboutButtonRow(QWidget *parent = nullptr):
        QWidget(parent),
        m_layout(new ElaFlowLayout(this, 0, 8, 8))
    {
        m_layout->setIsAnimation(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    void addButton(QWidget *button)
    {
        m_layout->addWidget(button);
    }

private:
    ElaFlowLayout *m_layout;

    Q_DISABLE_COPY_MOVE(ResponsiveAboutButtonRow)
};

void configureAboutButton(ElaPushButton *button, bool primary = false)
{
    if (button == nullptr) {
        return;
    }
    int textWidth = button->fontMetrics().horizontalAdvance(button->text());
    button->setMinimumWidth(qMax(120, textWidth + 40));
    button->setMinimumHeight(38);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAccessibleName(button->text());
    if (primary) {
        button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
        button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
        button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
        button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
        button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
        button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
        button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
        button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
    }
    else {
        button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, WindowBase));
        button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, BasicHover));
        button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, BasicPress));
        button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicText));
        button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, WindowBase));
        button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, BasicHover));
        button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, BasicPress));
        button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicText));
    }
}

QLabel *makeAboutLabel(QWidget *parent, QString const& text,
    QString const& objectName = QString())
{
    auto *label = new QLabel(text, parent);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QLabel *makeAboutLinkLabel(QWidget *parent, QString const& html)
{
    auto *label = new QLabel(html, parent);
    label->setObjectName(QStringLiteral("aboutLinkLabel"));
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    return label;
}

AboutCardHeader *makeAboutCard(QWidget *parent, QString const& title,
    QWidget **bodyOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("aboutCard"));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new AboutCardHeader(title, card);
    layout->addWidget(header);
    auto *body = new QWidget(card);
    body->setObjectName(QStringLiteral("aboutCardBody"));
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(18, 14, 18, 18);
    bodyLayout->setSpacing(9);
    layout->addWidget(body);
    header->setBody(body);
    if (bodyOut != nullptr) {
        *bodyOut = body;
    }
    return header;
}

void applyAboutTheme(QWidget *page)
{
    if (page == nullptr) {
        return;
    }
    AboutColors colors = themedColors();
    page->setStyleSheet(QString(
        "#aboutPage { background: %1; border: none; color: %6; }"
        "#aboutPage QWidget#qt_scrollarea_viewport { background: %2; border: none; }"
        "#aboutContent { background: %3; }"
        "#aboutIdentity, #aboutCard { background-color: %4;"
        " border: 1px solid %5; border-radius: 10px; }"
        "#aboutCardHeader { background: transparent; border: none;"
        " border-radius: 9px; color: %6; padding: 8px 14px;"
        " text-align: left; font-weight: 600; }"
        "#aboutCardHeader:hover { background-color: %10; }"
        "#aboutCardHeader:checked { background-color: %10; }"
        "#aboutCardHeader:focus { border: 2px solid %12; }"
        "#aboutCardBody { background-color: %4; border-top: 1px solid %5; }"
        "#aboutPage QLabel { background: transparent; color: %6; }"
        "#aboutPage QLabel#aboutSecondaryLabel { color: %7; }"
        "#aboutPage QLabel#aboutLinkLabel { color: %8; }"
        "#aboutPage QLabel#aboutLinkLabel a { color: %8; }"
        "#aboutPage QLabel#aboutStatusLabel { color: %6; font-weight: 600; }"
        "#aboutPage QLabel#aboutDisabledLabel { color: %9; }"
        "#aboutPage QPushButton:disabled { color: %9; background-color: %11;"
        " border: 1px solid %5; }"
        "#aboutPage QScrollBar:vertical { background: %3; width: 9px; }"
        "#aboutPage QScrollBar::handle:vertical { background: %13;"
        " min-height: 28px; border-radius: 4px; }"
        "#aboutPage QScrollBar::add-line:vertical,"
        "#aboutPage QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(colorName(colors.page), colorName(colors.viewport), colorName(colors.content),
        colorName(colors.card), colorName(colors.border), colorName(colors.text),
        colorName(colors.secondary), colorName(colors.link), colorName(colors.disabled),
        colorName(colors.hover), colorName(colors.disabledSurface), colorName(colors.focus),
        colorName(colors.scrollHandle)));

    QPalette palette = page->palette();
    palette.setColor(QPalette::Window, colors.page);
    palette.setColor(QPalette::Base, colors.content);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::WindowText, colors.text);
    palette.setColor(QPalette::Disabled, QPalette::Text, colors.disabled);
    page->setPalette(palette);
    page->setAutoFillBackground(true);
    if (auto *scroll = qobject_cast<QAbstractScrollArea *>(page)) {
        QPalette viewportPalette = palette;
        viewportPalette.setColor(QPalette::Window, colors.viewport);
        viewportPalette.setColor(QPalette::Base, colors.viewport);
        scroll->viewport()->setPalette(viewportPalette);
        scroll->viewport()->setAutoFillBackground(true);
    }
    if (auto *content = page->findChild<QWidget *>(QStringLiteral("aboutContent"))) {
        QPalette contentPalette = palette;
        contentPalette.setColor(QPalette::Window, colors.content);
        contentPalette.setColor(QPalette::Base, colors.content);
        content->setPalette(contentPalette);
        content->setAutoFillBackground(true);
    }
    for (auto *button : page->findChildren<QPushButton *>()) {
        if (auto *header = dynamic_cast<AboutCardHeader *>(button)) {
            header->updateIcon();
        }
        if (auto *aboutButton = dynamic_cast<AboutPushButton *>(button)) {
            configureAboutButton(aboutButton,
                aboutButton->property("aboutPrimary").toBool());
        }
    }
}

} // namespace

void ShijimaManager::setupAboutPage()
{
    auto *page = new ElaScrollArea(this);
    page->setObjectName(QStringLiteral("aboutPage"));
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    page->setFocusPolicy(Qt::StrongFocus);
    m_ui->aboutPage = page;

    auto *content = new AboutContentWidget(page);
    content->setObjectName(QStringLiteral("aboutContent"));
    auto *outer = new QHBoxLayout(content);
    outer->setContentsMargins(20, 18, 20, 24);
    outer->setSpacing(0);
    auto *container = new QWidget(content);
    container->setObjectName(QStringLiteral("aboutContainer"));
    constexpr int aboutMaximumContentWidth = 720;
    container->setMaximumWidth(aboutMaximumContentWidth);
    container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    auto *root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);
    root->setAlignment(Qt::AlignTop);
    outer->addStretch(1);
    outer->addWidget(container);
    outer->addStretch(1);
    content->setContainer(container,
        outer->contentsMargins().left() + outer->contentsMargins().right(),
        aboutMaximumContentWidth);
    page->setWidget(content);

    auto *title = new ElaText(tr("About"), container);
    title->setTextPixelSize(24);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    root->addWidget(title);

    auto *identity = new QFrame(container);
    identity->setObjectName(QStringLiteral("aboutIdentity"));
    auto *identityLayout = new QVBoxLayout(identity);
    identityLayout->setContentsMargins(20, 18, 20, 18);
    identityLayout->setSpacing(7);
    auto *iconLabel = new QLabel(identity);
    QIcon appIcon = qApp->windowIcon();
    if (!appIcon.isNull()) {
        iconLabel->setPixmap(appIcon.pixmap(72, 72));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(iconLabel);
    auto *identityTitle = new ElaText(tr("NeurolingsCE"), identity);
    identityTitle->setTextPixelSize(22);
    identityTitle->setAlignment(Qt::AlignCenter);
    identityTitle->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    identityLayout->addWidget(identityTitle);
    auto *description = makeAboutLabel(identity, tr(
        "A cross-platform shimeji desktop pet runner."),
        QStringLiteral("aboutSecondaryLabel"));
    description->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(description);
    auto *copyright = makeAboutLabel(identity, tr(
        "Copyright © 2025 pixelomer and contributors."),
        QStringLiteral("aboutSecondaryLabel"));
    copyright->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(copyright);
    auto *project = makeAboutLinkLabel(identity, QString());
    project->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(project);
    root->addWidget(identity);

    QWidget *versionBody = nullptr;
    auto *versionHeader = makeAboutCard(container, tr("Version"), &versionBody);
    root->addWidget(versionHeader->parentWidget());
    auto *versionLayout = versionBody->layout();
    QString version = QStringLiteral(NEUROLINGSCE_VERSION);
    auto *currentCaption = makeAboutLabel(versionBody, tr("Current Version"));
    auto *currentValue = makeAboutLabel(versionBody, QStringLiteral("v%1").arg(version));
    currentValue->setObjectName(QStringLiteral("aboutSecondaryLabel"));
    versionLayout->addWidget(currentCaption);
    versionLayout->addWidget(currentValue);
    auto *latestCaption = makeAboutLabel(versionBody, tr("Latest Release"));
    auto *latestValue = makeAboutLabel(versionBody, tr("Not checked yet"));
    latestValue->setObjectName(QStringLiteral("aboutSecondaryLabel"));
    versionLayout->addWidget(latestCaption);
    versionLayout->addWidget(latestValue);
    auto *copyRow = new ResponsiveAboutButtonRow(versionBody);
    auto *copyButton = new AboutPushButton(tr("Copy Version Info"), versionBody);
    copyButton->setAccessibleDescription(tr("Copy the current and latest version information."));
    configureAboutButton(copyButton);
    copyRow->addButton(copyButton);
    auto *copyStatus = makeAboutLabel(versionBody, QString(),
        QStringLiteral("aboutSecondaryLabel"));
    versionLayout->addWidget(copyRow);
    versionLayout->addWidget(copyStatus);

    QWidget *updateBody = nullptr;
    auto *updateHeader = makeAboutCard(container, tr("Updates"), &updateBody);
    root->addWidget(updateHeader->parentWidget());
    auto *updateLayout = updateBody->layout();
    auto *statusLabel = makeAboutLabel(updateBody, QString(),
        QStringLiteral("aboutStatusLabel"));
    auto *detailLabel = makeAboutLabel(updateBody, QString());
    detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *publishedLabel = makeAboutLabel(updateBody, QString(),
        QStringLiteral("aboutSecondaryLabel"));
    auto *notesLabel = makeAboutLabel(updateBody, tr("Release Notes"));
    notesLabel->setObjectName(QStringLiteral("aboutSecondaryLabel"));
    updateLayout->addWidget(statusLabel);
    updateLayout->addWidget(detailLabel);
    updateLayout->addWidget(publishedLabel);
    updateLayout->addWidget(notesLabel);
    auto *primaryButtons = new ResponsiveAboutButtonRow(updateBody);
    auto *checkButton = new AboutPushButton(tr("Check for Updates"), updateBody);
    checkButton->setAccessibleDescription(tr("Check GitHub for a newer NeurolingsCE release."));
    configureAboutButton(checkButton, true);
    checkButton->setProperty("aboutPrimary", true);
    primaryButtons->addButton(checkButton);
    auto *releaseButton = new AboutPushButton(
        m_updateManager != nullptr ? m_updateManager->releaseButtonText()
            : tr("View Release Notes"), updateBody);
    releaseButton->setAccessibleDescription(tr("Open the release notes in your browser."));
    configureAboutButton(releaseButton);
    primaryButtons->addButton(releaseButton);
    auto *installButton = new AboutPushButton(updateBody);
    installButton->setAccessibleDescription(tr("Download or install the selected update."));
    configureAboutButton(installButton, true);
    installButton->setProperty("aboutPrimary", true);
    primaryButtons->addButton(installButton);
    updateLayout->addWidget(primaryButtons);
    auto *secondaryButtons = new ResponsiveAboutButtonRow(updateBody);
    auto *ignoreButton = new AboutPushButton(tr("Ignore This Version"), updateBody);
    ignoreButton->setAccessibleDescription(tr("Ignore this release until a newer version is available."));
    configureAboutButton(ignoreButton);
    secondaryButtons->addButton(ignoreButton);
    auto *laterButton = new AboutPushButton(tr("Remind Me Later"), updateBody);
    laterButton->setAccessibleDescription(tr("Temporarily hide this update reminder."));
    configureAboutButton(laterButton);
    secondaryButtons->addButton(laterButton);
    updateLayout->addWidget(secondaryButtons);

    QWidget *projectBody = nullptr;
    auto *projectHeader = makeAboutCard(container, tr("Project & Support"), &projectBody);
    root->addWidget(projectHeader->parentWidget());
    auto *projectLayout = projectBody->layout();
    auto *authorLabel = makeAboutLinkLabel(projectBody, QString());
    auto *upstreamLabel = makeAboutLinkLabel(projectBody, QString());
    auto *githubLabel = makeAboutLinkLabel(projectBody, QString());
    auto *qqLabel = makeAboutLabel(projectBody, QString());
    qqLabel->setObjectName(QStringLiteral("aboutSecondaryLabel"));
    auto *licenseLabel = makeAboutLabel(projectBody, QString());
    licenseLabel->setObjectName(QStringLiteral("aboutSecondaryLabel"));
    projectLayout->addWidget(authorLabel);
    projectLayout->addWidget(upstreamLabel);
    projectLayout->addWidget(githubLabel);
    projectLayout->addWidget(qqLabel);
    projectLayout->addWidget(licenseLabel);
    auto *supportButtons = new ResponsiveAboutButtonRow(projectBody);
    auto *licensesButton = new AboutPushButton(tr("View Licenses"), projectBody);
    licensesButton->setAccessibleDescription(tr("View the licenses for NeurolingsCE and its dependencies."));
    configureAboutButton(licensesButton);
    supportButtons->addButton(licensesButton);
    auto *issueButton = new AboutPushButton(tr("Report Issue"), projectBody);
    issueButton->setAccessibleDescription(tr("Open the NeurolingsCE issue tracker."));
    configureAboutButton(issueButton);
    supportButtons->addButton(issueButton);
    projectLayout->addWidget(supportButtons);
    root->addStretch(1);

    auto refreshAboutText = [this, project, authorLabel,
        upstreamLabel, githubLabel, qqLabel, licenseLabel]() mutable {
        AboutColors current = themedColors();
        project->setText(tr("Project: %1").arg(linkHtml(
            tr("NeurolingsCE"),
            QStringLiteral("https://github.com/qingchenyouforcc/NeurolingsCE"),
            current.link)));
        authorLabel->setText(tr("Author: %1").arg(linkHtml(
            tr("Qingchen You"),
            QStringLiteral("https://space.bilibili.com/178381315"), current.link)));
        upstreamLabel->setText(tr("Upstream: %1").arg(linkHtml(
            tr("Shijima-Qt"), QStringLiteral("https://github.com/pixelomer/Shijima-Qt"),
            current.link) + tr(" by %1").arg(tr("pixelomer"))));
        githubLabel->setText(tr("GitHub: %1").arg(linkHtml(
            tr("NeurolingsCE"),
            QStringLiteral("https://github.com/qingchenyouforcc/NeurolingsCE"),
            current.link)));
        qqLabel->setText(tr("QQ Group: %1").arg(QStringLiteral("125081756")));
        licenseLabel->setText(tr("License: %1").arg(tr("GPLv3")));
    };

    auto refreshUpdateCard = [this, latestValue, statusLabel, detailLabel,
        publishedLabel, checkButton, releaseButton, installButton,
        ignoreButton, laterButton]() {
        if (m_updateManager == nullptr) {
            latestValue->setText(tr("Unavailable"));
            statusLabel->setText(tr("Update service is unavailable."));
            detailLabel->clear();
            publishedLabel->clear();
            checkButton->setEnabled(false);
            releaseButton->setEnabled(false);
            installButton->setEnabled(false);
            ignoreButton->setEnabled(false);
            laterButton->setEnabled(false);
            return;
        }
        latestValue->setText(m_updateManager->latestVersion().isEmpty()
            ? tr("Not checked yet")
            : QStringLiteral("v%1").arg(m_updateManager->latestVersion()));
        statusLabel->setText(m_updateManager->statusText());
        detailLabel->setText(m_updateManager->detailText());
        publishedLabel->setText(m_updateManager->publishedAtText());
        checkButton->setEnabled(m_updateManager->canCheckForUpdates());
        releaseButton->setText(m_updateManager->releaseButtonText());
        configureAboutButton(releaseButton);
        releaseButton->setEnabled(m_updateManager->hasRelease());
        installButton->setText(m_updateManager->installButtonText());
        configureAboutButton(installButton, true);
        installButton->setProperty("aboutPrimary", true);
        installButton->setEnabled(m_updateManager->canDownloadInstaller()
            || m_updateManager->canInstallDownloadedUpdate());
        ignoreButton->setEnabled(m_updateManager->shouldShowIgnoreActions());
        laterButton->setEnabled(m_updateManager->shouldShowIgnoreActions());
    };

    connect(copyButton, &ElaPushButton::clicked, this,
        [this, copyStatus, version, latestValue]() {
            QString latest = latestValue->text();
            QGuiApplication::clipboard()->setText(
                tr("NeurolingsCE %1 (latest: %2)").arg(version, latest));
            copyStatus->setText(tr("Version information copied."));
        });
    connect(licensesButton, &ElaPushButton::clicked, this, [this]() {
        ShijimaLicensesDialog dialog { this };
        dialog.exec();
    });
    connect(issueButton, &ElaPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl {
            QStringLiteral("https://github.com/qingchenyouforcc/NeurolingsCE/issues") });
    });
    connect(checkButton, &ElaPushButton::clicked, this, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->checkForUpdates(GitHubUpdateManager::CheckMode::Manual);
        }
    });
    connect(releaseButton, &ElaPushButton::clicked, this, [this]() {
        if (m_updateManager != nullptr && !m_updateManager->releaseUrl().isEmpty()) {
            QDesktopServices::openUrl(QUrl { m_updateManager->releaseUrl() });
        }
    });
    connect(installButton, &ElaPushButton::clicked, this, [this]() {
        if (m_updateManager == nullptr) {
            return;
        }
        if (m_updateManager->canInstallDownloadedUpdate()) {
            QString verificationError;
            if (!m_updateManager->verifyDownloadedInstaller(verificationError)) {
                ShijimaManagerUiInternal::showThemedWarning(
                    this, tr("Install Update"), verificationError);
                if (!m_updateManager->releaseUrl().isEmpty()) {
                    QDesktopServices::openUrl(QUrl { m_updateManager->releaseUrl() });
                }
                return;
            }
            QString versionText = QStringLiteral("v%1").arg(m_updateManager->latestVersion());
            if (!ShijimaManagerUiInternal::showThemedQuestion(
                this, tr("Install Update"),
                tr("Install %1 now? NeurolingsCE will close before the installer starts.")
                    .arg(versionText))) {
                return;
            }
            bool launched = false;
            QString installerPath = m_updateManager->downloadedInstallerPath();
#ifdef _WIN32
            if (m_updateManager->assetKind() == GitHubUpdateManager::AssetKind::Msi) {
                launched = QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath));
            }
            else if (m_updateManager->assetKind() == GitHubUpdateManager::AssetKind::Exe) {
                launched = QProcess::startDetached(installerPath, {});
            }
#else
            launched = QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath));
#endif
            if (!launched) {
                ShijimaManagerUiInternal::showThemedWarning(
                    this, tr("Install Update"),
                    tr("The installer could not be started. You can open the release page and install manually."));
                return;
            }
            m_localApi->stop();
            m_httpApi->stop();
            m_allowClose = true;
            closeManagerWindow();
            return;
        }
        if (m_updateManager->canDownloadInstaller()) {
            m_updateManager->downloadAndPrepareUpdate();
            return;
        }
        if (!m_updateManager->releaseUrl().isEmpty()) {
            QDesktopServices::openUrl(QUrl { m_updateManager->releaseUrl() });
        }
    });
    connect(ignoreButton, &ElaPushButton::clicked, this, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->ignoreLatestVersion();
        }
    });
    connect(laterButton, &ElaPushButton::clicked, this, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->remindLater();
        }
    });
    if (m_updateManager != nullptr) {
        connect(m_updateManager, &GitHubUpdateManager::stateChanged,
            page, refreshUpdateCard);
    }

    refreshAboutText();
    refreshUpdateCard();
    applyAboutTheme(page);
    connect(eTheme, &ElaTheme::themeModeChanged, page,
        [page, refreshAboutText]() mutable {
            applyAboutTheme(page);
            refreshAboutText();
        });

    addFooterNode(tr("About"), page, m_ui->aboutKey, 0, ElaIconType::User);
}

void ShijimaManager::showAboutPage()
{
    setManagerVisible(true);
    if (!m_ui->aboutKey.isEmpty()) {
        navigation(m_ui->aboutKey);
    }
}
