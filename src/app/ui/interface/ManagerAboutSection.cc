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
#include <QDesktopServices>
#include <QDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>
#include "shijima-qt/ui/dialogs/licenses/ShijimaLicensesDialog.hpp"
#include "ElaTheme.h"

#ifndef NEUROLINGSCE_VERSION
#define NEUROLINGSCE_VERSION "0.1.0"
#endif

namespace {

struct AboutColors {
    QString primary;
    QString text;
    QString details;
    QString dialogBg;
    QString badgeBg;
    QString divider;
    QString subtleBg;
};

AboutColors themedColors()
{
    auto themeMode = eTheme->getThemeMode();
    return AboutColors {
        ElaThemeColor(themeMode, PrimaryNormal).name(),
        ElaThemeColor(themeMode, BasicText).name(),
        ElaThemeColor(themeMode, BasicDetailsText).name(),
        ElaThemeColor(themeMode, DialogBase).name(),
        ElaThemeColor(themeMode, BasicHover).name(),
        ElaThemeColor(themeMode, BasicBorder).name(),
        ElaThemeColor(themeMode, WindowBase).name(),
    };
}

void configureDialogButton(QPushButton *button)
{
    const QFontMetrics metrics(button->font());
    const int horizontalPadding = 44;
    button->setMinimumWidth(metrics.horizontalAdvance(button->text()) + horizontalPadding);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

QLabel *makeSectionTitle(QString const& text, QString const& color)
{
    auto *label = new QLabel(QStringLiteral(
        "<span style='color: %1; font-size: 16px; font-weight: 600;'>%2</span>")
        .arg(color, text.toHtmlEscaped()));
    label->setTextFormat(Qt::RichText);
    return label;
}

}

void ShijimaManager::setupAboutPage()
{
    addFooterNode(tr("About"), m_ui->aboutKey, 0, ElaIconType::User);
    connect(this, &ElaWindow::navigationNodeClicked,
        [this](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) {
            if (nodeKey != m_ui->aboutKey) {
                return;
            }
            Q_UNUSED(nodeType);
            showAboutDialog();
        });
}

void ShijimaManager::showAboutDialog()
{
    QString version = QStringLiteral(NEUROLINGSCE_VERSION);
    QString authorName = QString::fromUtf8("\xe8\xbd\xbb\xe5\xb0\x98\xe5\x91\xa6");
    AboutColors colors = themedColors();

    QDialog *aboutDialog = new QDialog(this);
    aboutDialog->setWindowTitle(tr("About NeurolingsCE"));
    aboutDialog->setMinimumWidth(860);
    aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
    aboutDialog->setStyleSheet(QString("QDialog { background-color: %1; }").arg(colors.dialogBg));

    QString buttonStyle = QString(
        "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
        "border-radius: 6px; padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:pressed { background-color: %5; }"
        "QPushButton:disabled { color: %6; background-color: %1; border-color: %3; }"
    ).arg(colors.subtleBg, colors.text, colors.divider,
         ElaThemeColor(eTheme->getThemeMode(), BasicHover).name(),
         ElaThemeColor(eTheme->getThemeMode(), BasicPress).name(),
         colors.details);

    auto *rootLayout = new QVBoxLayout;
    rootLayout->setSpacing(16);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    aboutDialog->setLayout(rootLayout);

    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(28);
    rootLayout->addLayout(contentRow);

    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setSpacing(14);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    contentRow->addWidget(leftPane, 6);

    QLabel *iconLabel = new QLabel;
    QIcon appIcon = qApp->windowIcon();
    if (!appIcon.isNull()) {
        iconLabel->setPixmap(appIcon.pixmap(72, 72));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(iconLabel);

    QLabel *titleLabel = new QLabel(
        QString("<h2 style='color: %1; margin: 0;'>NeurolingsCE</h2>").arg(colors.primary));
    titleLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(titleLabel);

    QLabel *versionLabel = new QLabel(
        QString("<span style='background-color: %1; color: %2; "
            "padding: 3px 12px; border-radius: 10px; font-size: 12px;'>v%3</span>")
        .arg(colors.badgeBg, colors.primary, version));
    versionLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(versionLabel);

    QLabel *descLabel = new QLabel(tr("A cross-platform shimeji desktop pet runner."));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QString("color: %1; margin: 4px 0;").arg(colors.details));
    leftLayout->addWidget(descLabel);

    QString infoHtml = QStringLiteral(
        "<table cellpadding='4' style='color: %1;'>"
        "<tr><td style='color: %2; font-weight: bold;'>%3</td>"
        "<td><a href='https://space.bilibili.com/178381315' "
        "style='color: %2; text-decoration: none;'>%4</a></td></tr>"
        "<tr><td style='color: %2; font-weight: bold;'>%5</td>"
        "<td><a href='https://github.com/pixelomer/Shijima-Qt' "
        "style='color: %2; text-decoration: none;'>Shijima-Qt</a> by pixelomer</td></tr>"
        "<tr><td style='color: %2; font-weight: bold;'>%6</td>"
        "<td><a href='https://github.com/qingchenyouforcc/NeurolingsCE' "
        "style='color: %2; text-decoration: none;'>GitHub</a></td></tr>"
        "<tr><td style='color: %2; font-weight: bold;'>%7</td>"
        "<td>423902950</td></tr>"
        "<tr><td style='color: %2; font-weight: bold;'>%8</td>"
        "<td>125081756</td></tr>"
        "</table>")
        .arg(colors.text, colors.primary,
             tr("Author"), authorName,
             tr("Based on"),
             tr("Project"),
             tr("Feedback QQ"),
             tr("Chat QQ"));
    QLabel *infoLabel = new QLabel(infoHtml);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QString(
        "background-color: transparent; border: none; color: %1;").arg(colors.text));
    leftLayout->addWidget(infoLabel);

    auto *leftButtonsRow = new QHBoxLayout;
    leftButtonsRow->setSpacing(8);
    QPushButton *licensesButton = new QPushButton(tr("View Licenses"));
    licensesButton->setStyleSheet(buttonStyle);
    configureDialogButton(licensesButton);
    connect(licensesButton, &QPushButton::clicked, [this]() {
        ShijimaLicensesDialog dialog { this };
        dialog.exec();
    });
    leftButtonsRow->addWidget(licensesButton);
    leftButtonsRow->addStretch();

    QPushButton *btnBug = new QPushButton(tr("Report Issue"));
    btnBug->setStyleSheet(buttonStyle);
    configureDialogButton(btnBug);
    connect(btnBug, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl { "https://github.com/qingchenyouforcc/NeurolingsCE/issues" });
    });
    leftButtonsRow->addWidget(btnBug);
    leftLayout->addLayout(leftButtonsRow);
    leftLayout->addStretch();

    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setStyleSheet(QString("color: %1; background-color: %1;").arg(colors.divider));
    contentRow->addWidget(divider);

    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setSpacing(14);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    contentRow->addWidget(rightPane, 5);

    rightLayout->addWidget(makeSectionTitle(tr("Updates"), colors.primary));

    auto *versionGrid = new QGridLayout;
    versionGrid->setHorizontalSpacing(12);
    versionGrid->setVerticalSpacing(8);
    QLabel *currentVersionTitle = new QLabel(tr("Current Version"));
    QLabel *latestVersionTitle = new QLabel(tr("Latest Release"));
    QLabel *currentVersionValue = new QLabel(QStringLiteral("v%1").arg(version));
    QLabel *latestVersionValue = new QLabel(tr("Not checked yet"));
    for (QLabel *label : { currentVersionTitle, latestVersionTitle, currentVersionValue, latestVersionValue }) {
        label->setStyleSheet(QString("color: %1;").arg(colors.text));
        label->setWordWrap(true);
    }
    currentVersionTitle->setStyleSheet(QString("color: %1; font-weight: 600;").arg(colors.text));
    latestVersionTitle->setStyleSheet(QString("color: %1; font-weight: 600;").arg(colors.text));
    versionGrid->addWidget(currentVersionTitle, 0, 0);
    versionGrid->addWidget(currentVersionValue, 0, 1);
    versionGrid->addWidget(latestVersionTitle, 1, 0);
    versionGrid->addWidget(latestVersionValue, 1, 1);
    rightLayout->addLayout(versionGrid);

    QLabel *statusLabel = new QLabel;
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet(QString(
        "color: %1; font-size: 15px; font-weight: 600;").arg(colors.primary));
    rightLayout->addWidget(statusLabel);

    QLabel *detailLabel = new QLabel;
    detailLabel->setWordWrap(true);
    detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailLabel->setStyleSheet(QString(
        "color: %1; background-color: transparent; border: none;").arg(colors.text));
    rightLayout->addWidget(detailLabel);

    QLabel *publishedLabel = new QLabel;
    publishedLabel->setWordWrap(true);
    publishedLabel->setStyleSheet(QString("color: %1;").arg(colors.details));
    rightLayout->addWidget(publishedLabel);

    auto *primaryButtonsRow = new QHBoxLayout;
    primaryButtonsRow->setSpacing(8);
    QPushButton *checkButton = new QPushButton(tr("Check for Updates"));
    QPushButton *releaseButton = new QPushButton(
        m_updateManager != nullptr ? m_updateManager->releaseButtonText() : tr("View Release Notes"));
    QPushButton *installButton = new QPushButton;
    for (QPushButton *button : { checkButton, releaseButton, installButton }) {
        button->setStyleSheet(buttonStyle);
        configureDialogButton(button);
        primaryButtonsRow->addWidget(button);
    }
    rightLayout->addLayout(primaryButtonsRow);

    auto *secondaryButtonsRow = new QHBoxLayout;
    secondaryButtonsRow->setSpacing(8);
    QPushButton *ignoreButton = new QPushButton(tr("Ignore This Version"));
    QPushButton *laterButton = new QPushButton(tr("Remind Me Later"));
    for (QPushButton *button : { ignoreButton, laterButton }) {
        button->setStyleSheet(buttonStyle);
        configureDialogButton(button);
        secondaryButtonsRow->addWidget(button);
    }
    secondaryButtonsRow->addStretch();
    rightLayout->addLayout(secondaryButtonsRow);
    rightLayout->addStretch();

    auto refreshUpdateCard = [this, latestVersionValue, statusLabel, detailLabel, publishedLabel,
                              checkButton, releaseButton, installButton, ignoreButton, laterButton]() {
        if (m_updateManager == nullptr) {
            latestVersionValue->setText(tr("Unavailable"));
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

        if (m_updateManager->latestVersion().isEmpty()) {
            latestVersionValue->setText(tr("Not checked yet"));
        }
        else {
            latestVersionValue->setText(QStringLiteral("v%1").arg(m_updateManager->latestVersion()));
        }

        statusLabel->setText(m_updateManager->statusText());
        detailLabel->setText(m_updateManager->detailText());
        publishedLabel->setText(m_updateManager->publishedAtText());

        checkButton->setEnabled(m_updateManager->canCheckForUpdates());
        releaseButton->setText(m_updateManager->releaseButtonText());
        releaseButton->setEnabled(m_updateManager->hasRelease());
        installButton->setText(m_updateManager->installButtonText());
        installButton->setEnabled(m_updateManager->canDownloadInstaller()
            || m_updateManager->canInstallDownloadedUpdate());
        ignoreButton->setEnabled(m_updateManager->shouldShowIgnoreActions());
        laterButton->setEnabled(m_updateManager->shouldShowIgnoreActions());
    };

    if (m_updateManager != nullptr) {
        connect(m_updateManager, &GitHubUpdateManager::stateChanged,
            aboutDialog, refreshUpdateCard);
    }

    connect(checkButton, &QPushButton::clicked, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->checkForUpdates(GitHubUpdateManager::CheckMode::Manual);
        }
    });
    connect(releaseButton, &QPushButton::clicked, [this]() {
        if (m_updateManager != nullptr && !m_updateManager->releaseUrl().isEmpty()) {
            QDesktopServices::openUrl(QUrl { m_updateManager->releaseUrl() });
        }
    });
    connect(installButton, &QPushButton::clicked, aboutDialog, [this, aboutDialog]() {
        if (m_updateManager == nullptr) {
            return;
        }

        if (m_updateManager->canInstallDownloadedUpdate()) {
            QString versionText = QStringLiteral("v%1").arg(m_updateManager->latestVersion());
            int result = QMessageBox::question(
                aboutDialog,
                tr("Install Update"),
                tr("Install %1 now? NeurolingsCE will close before the installer starts.")
                    .arg(versionText),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (result != QMessageBox::Yes) {
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
                QMessageBox::warning(
                    aboutDialog,
                    tr("Install Update"),
                    tr("The installer could not be started. You can open the release page and install manually."));
                return;
            }

            m_localApi->stop();
            m_httpApi->stop();
            m_allowClose = true;
            aboutDialog->accept();
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
    connect(ignoreButton, &QPushButton::clicked, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->ignoreLatestVersion();
        }
    });
    connect(laterButton, &QPushButton::clicked, [this]() {
        if (m_updateManager != nullptr) {
            m_updateManager->remindLater();
        }
    });

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    QPushButton *closeButton = new QPushButton(tr("Close"));
    closeButton->setStyleSheet(buttonStyle);
    configureDialogButton(closeButton);
    connect(closeButton, &QPushButton::clicked, aboutDialog, &QDialog::accept);
    buttonRow->addWidget(closeButton);
    rootLayout->addLayout(buttonRow);

    refreshUpdateCard();
    aboutDialog->exec();
}
