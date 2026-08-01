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
#include "../../runtime/ManagerRuntimeState.hpp"
#include "../ManagerUiState.hpp"
#include "../ManagerUiHelpers.hpp"
#include <QBoxLayout>
#include <QDesktopServices>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyleOptionFocusRect>
#include <QUrl>
#include <QVBoxLayout>
#include "ElaFlowLayout.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"

namespace {

constexpr int kCompactHomeWidth = 640;

void drawHomeFocusFrame(QWidget *widget)
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

class HomePushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawHomeFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(HomePushButton)
};

class HomeToolButton final : public ElaToolButton {
public:
    explicit HomeToolButton(QWidget *parent = nullptr):
        ElaToolButton(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaToolButton::paintEvent(event);
        drawHomeFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(HomeToolButton)
};

class ResponsiveHomeContent final : public QWidget {
public:
    ResponsiveHomeContent(QWidget *library, QWidget *details, QWidget *parent = nullptr):
        QWidget(parent),
        m_details(details),
        m_layout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(10);
        m_layout->addWidget(library, 1);
        m_layout->addWidget(details);
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
        bool compact = width() > 0 && width() < kCompactHomeWidth;
        m_layout->setDirection(compact
            ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_details->setMinimumWidth(compact ? 0 : 220);
        m_details->setMaximumWidth(compact ? QWIDGETSIZE_MAX : 300);
    }

    QWidget *m_details;
    QBoxLayout *m_layout;

    Q_DISABLE_COPY_MOVE(ResponsiveHomeContent)
};

void configureHomeButton(QWidget *button, QString const& accessibleName,
    QString const& accessibleDescription, int iconWidth = 0)
{
    button->setMinimumHeight(38);
    constexpr int horizontalPadding = 32;
    button->setMinimumWidth(button->fontMetrics().horizontalAdvance(accessibleName)
        + iconWidth + horizontalPadding);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(accessibleName);
    button->setAccessibleDescription(accessibleDescription);
    button->setToolTip(accessibleDescription);
}

void configurePrimaryButton(HomePushButton *button, QString const& description)
{
    configureHomeButton(button, button->text(), description);
    button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
    button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
    button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
    button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
    button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
    button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
    button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
}

HomeToolButton *createHomeCommand(QWidget *parent, QString const& text,
    QString const& description, ElaIconType::IconName icon)
{
    auto *button = new HomeToolButton(parent);
    button->setText(text);
    button->setElaIcon(icon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIsTransparent(false);
    configureHomeButton(button, text, description, button->iconSize().width());
    return button;
}

}

static QLabel *createMetaLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setProperty("muted", true);
    return label;
}

static void applyHomeTheme(QWidget *homePage)
{
    auto mode = eTheme->getThemeMode();
    QColor panel = eTheme->getThemeColor(mode, ElaThemeType::BasicBase);
    QColor border = eTheme->getThemeColor(mode, ElaThemeType::BasicBorder);
    QColor text = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
    QColor muted = eTheme->getThemeColor(mode, ElaThemeType::BasicDetailsText);
    QColor preview = eTheme->getThemeColor(mode, ElaThemeType::WindowBase);

    homePage->setStyleSheet(QString(
        "#homeActionBar, #mascotDetailsPanel, #mascotLibrarySurface {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "#homeSectionTitle {"
        "  color: %3;"
        "  font-weight: 600;"
        "}"
        "#homePageDescription, #homeEmptyDescription {"
        "  color: %4;"
        "}"
        "#mascotPreview {"
        "  background-color: %5;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel[muted=\"true\"] {"
        "  color: %4;"
        "}"
    ).arg(panel.name(QColor::HexArgb), border.name(QColor::HexArgb),
        text.name(QColor::HexArgb), muted.name(QColor::HexArgb),
        preview.name(QColor::HexArgb)));
}

void ShijimaManager::setupHomePage() {
    m_ui->homePage = new QWidget(this);
    auto *homeLayout = new QVBoxLayout(m_ui->homePage);
    homeLayout->setContentsMargins(16, 14, 16, 14);
    homeLayout->setSpacing(12);

    auto *pageTitle = new ElaText(tr("Mascot Manager"), m_ui->homePage);
    pageTitle->setTextPixelSize(20);
    pageTitle->setWordWrap(false);
    pageTitle->setStyleSheet(QStringLiteral(
        "#ElaText { background-color: transparent; border: none; }"));
    homeLayout->addWidget(pageTitle);

    auto *pageDescription = new QLabel(
        tr("Browse your mascot library, start companions, and manage installed packages."),
        m_ui->homePage);
    pageDescription->setObjectName(QStringLiteral("homePageDescription"));
    pageDescription->setWordWrap(true);
    homeLayout->addWidget(pageDescription);

    auto *actionBar = new QFrame(m_ui->homePage);
    actionBar->setObjectName(QStringLiteral("homeActionBar"));
    auto *actionFlow = new ElaFlowLayout(actionBar, 10, 8, 8);
    actionFlow->setIsAnimation(false);

    auto *btnSpawn = new HomePushButton(tr("Spawn Random"), actionBar);
    configurePrimaryButton(btnSpawn,
        tr("Spawn a random mascot from the library."));
    m_ui->spawnRandomButton = btnSpawn;
    connect(btnSpawn, &ElaPushButton::clicked, this, &ShijimaManager::spawnClicked);
    actionFlow->addWidget(btnSpawn);

    auto *btnImport = createHomeCommand(actionBar, tr("Import"),
        tr("Import mascot packages or Shimeji archives."), ElaIconType::FileImport);
    connect(btnImport, &ElaToolButton::clicked, this, &ShijimaManager::importAction);
    actionFlow->addWidget(btnImport);

    auto *btnRefresh = createHomeCommand(actionBar, tr("Refresh"),
        tr("Reload mascot packages from the library folder."), ElaIconType::ArrowsRotate);
    connect(btnRefresh, &ElaToolButton::clicked,
        this, &ShijimaManager::syncMascotLibrary);
    actionFlow->addWidget(btnRefresh);

    auto *btnFolder = createHomeCommand(actionBar, tr("Show Folder"),
        tr("Open the mascot library folder."), ElaIconType::FolderOpen);
    connect(btnFolder, &ElaToolButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_runtime->mascotsPath));
    });
    actionFlow->addWidget(btnFolder);
    homeLayout->addWidget(actionBar);

    auto *libraryColumn = new QWidget(m_ui->homePage);
    auto *libraryLayout = new QVBoxLayout(libraryColumn);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(6);
    auto *libraryTitle = new QLabel(tr("Mascot Library"), libraryColumn);
    libraryTitle->setObjectName(QStringLiteral("homeSectionTitle"));
    libraryLayout->addWidget(libraryTitle);

    auto *librarySurface = new QFrame(libraryColumn);
    librarySurface->setObjectName(QStringLiteral("mascotLibrarySurface"));
    auto *librarySurfaceLayout = new QVBoxLayout(librarySurface);
    librarySurfaceLayout->setContentsMargins(8, 8, 8, 8);

    m_ui->listWidget->setParent(librarySurface);
    m_ui->listWidget->setUniformItemSizes(true);
    m_ui->listWidget->setAccessibleName(tr("Mascot Library"));
    m_ui->listWidget->setAccessibleDescription(
        tr("Installed mascot templates. Use arrow keys to select and Enter to spawn."));
    librarySurfaceLayout->addWidget(m_ui->listWidget, 1);

    auto *emptyState = new QWidget(librarySurface);
    m_ui->mascotEmptyStateWidget = emptyState;
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();

    auto *emptyTitle = new QLabel(tr("No imported mascots yet"), emptyState);
    emptyTitle->setObjectName(QStringLiteral("homeSectionTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    auto *emptyDescription = new QLabel(
        tr("Import a .mascot package or Shimeji archive to get started."), emptyState);
    emptyDescription->setObjectName(QStringLiteral("homeEmptyDescription"));
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setWordWrap(true);
    emptyLayout->addWidget(emptyDescription);

    auto *emptyImportButton = new HomePushButton(tr("Import Mascot..."), emptyState);
    configurePrimaryButton(emptyImportButton,
        tr("Import a mascot package or Shimeji archive."));
    connect(emptyImportButton, &ElaPushButton::clicked,
        this, &ShijimaManager::importAction);
    emptyLayout->addWidget(emptyImportButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    librarySurfaceLayout->addWidget(emptyState, 1);
    libraryLayout->addWidget(librarySurface, 1);

    auto *details = new QFrame(m_ui->homePage);
    details->setObjectName(QStringLiteral("mascotDetailsPanel"));
    m_ui->mascotDetailsPanel = details;
    details->setMinimumWidth(220);
    details->setMaximumWidth(300);
    auto *detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(12, 12, 12, 12);
    detailsLayout->setSpacing(8);

    auto *detailsTitle = new QLabel(tr("Details"), details);
    detailsTitle->setObjectName(QStringLiteral("homeSectionTitle"));
    detailsLayout->addWidget(detailsTitle);

    m_ui->mascotPreviewLabel = new QLabel(details);
    m_ui->mascotPreviewLabel->setObjectName(QStringLiteral("mascotPreview"));
    m_ui->mascotPreviewLabel->setFixedSize(96, 96);
    m_ui->mascotPreviewLabel->setAlignment(Qt::AlignCenter);
    detailsLayout->addWidget(m_ui->mascotPreviewLabel, 0, Qt::AlignHCenter);

    m_ui->mascotNameLabel = new QLabel(details);
    m_ui->mascotNameLabel->setWordWrap(true);
    m_ui->mascotNameLabel->setAlignment(Qt::AlignCenter);
    QFont nameFont = m_ui->mascotNameLabel->font();
    nameFont.setBold(true);
    if (nameFont.pointSize() > 0) {
        nameFont.setPointSize(nameFont.pointSize() + 1);
    }
    else if (nameFont.pixelSize() > 0) {
        nameFont.setPixelSize(nameFont.pixelSize() + 1);
    }
    else {
        nameFont.setPointSizeF(10.0);
    }
    m_ui->mascotNameLabel->setFont(nameFont);
    detailsLayout->addWidget(m_ui->mascotNameLabel);

    m_ui->mascotVersionLabel = createMetaLabel(details);
    detailsLayout->addWidget(m_ui->mascotVersionLabel);

    m_ui->mascotAuthorLabel = createMetaLabel(details);
    detailsLayout->addWidget(m_ui->mascotAuthorLabel);

    m_ui->mascotDescriptionLabel = createMetaLabel(details);
    m_ui->mascotDescriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailsLayout->addWidget(m_ui->mascotDescriptionLabel, 1);

    auto *btnDelete = createHomeCommand(details, tr("Delete Selected"),
        tr("Delete the selected mascot packages."), ElaIconType::TrashCan);
    btnDelete->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnDelete->setEnabled(false);
    m_ui->deleteMascotButton = btnDelete;
    connect(btnDelete, &ElaToolButton::clicked, this, &ShijimaManager::deleteAction);
    detailsLayout->addWidget(btnDelete);

    auto *responsiveContent = new ResponsiveHomeContent(
        libraryColumn, details, m_ui->homePage);
    homeLayout->addWidget(responsiveContent, 1);

    setTabOrder(btnSpawn, btnImport);
    setTabOrder(btnImport, btnRefresh);
    setTabOrder(btnRefresh, btnFolder);
    setTabOrder(btnFolder, m_ui->listWidget);
    setTabOrder(m_ui->listWidget, emptyImportButton);
    setTabOrder(emptyImportButton, btnDelete);
    applyHomeTheme(m_ui->homePage);
    connect(eTheme, &ElaTheme::themeModeChanged, m_ui->homePage, [this]() {
        applyHomeTheme(m_ui->homePage);
    });
    updateSelectedMascotDetails();

    addPageNode(tr("Home"), m_ui->homePage, ElaIconType::House);
}
