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
#include "shijima-qt/CredentialStore.hpp"
#include "shijima-qt/GitHubAuthManager.hpp"
#include "shijima-qt/MascotStoreCache.hpp"
#include "shijima-qt/MascotStoreConfig.hpp"
#include "shijima-qt/MascotStoreCoordinator.hpp"
#include "shijima-qt/MascotStoreIndex.hpp"
#include "shijima-qt/MascotStoreNetwork.hpp"
#include "shijima-qt/MascotSubmissionClient.hpp"
#include "shijima-qt/Secrets.hpp"
#include "../ManagerUiState.hpp"
#include "MascotStoreUi.hpp"
#include "../dialogs/submission/MascotSubmissionDialog.hpp"
#include "../dialogs/store/MascotStoreDetailDialog.hpp"

#include <QBoxLayout>
#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QColor>

#include <climits>
#include <memory>

#include "ElaIcon.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"

namespace {

int const kMascotIdRole = Qt::UserRole;

QString createTr(char const *sourceText) {
    return QCoreApplication::translate("ShijimaManager", sourceText);
}

void setStoreCardSelected(QListWidgetItem *item, bool selected)
{
    if (item == nullptr) {
        return;
    }
    auto *card = item->listWidget() == nullptr
        ? nullptr : item->listWidget()->itemWidget(item);
    if (card == nullptr) {
        return;
    }
    card->setProperty("selected", selected);
    card->style()->unpolish(card);
    card->style()->polish(card);
}

class StorePushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        if (hasFocus()) {
            QStyleOptionFocusRect option;
            option.initFrom(this);
            option.rect = rect().adjusted(3, 3, -3, -3);
            QPainter painter(this);
            style()->drawPrimitive(
                QStyle::PE_FrameFocusRect, &option, &painter, this);
        }
    }
};

void configureStoreButton(QPushButton *button, QString const& description)
{
    button->setMinimumHeight(36);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(button->text());
    button->setAccessibleDescription(description);
    button->setToolTip(description);
    button->setFocusPolicy(Qt::StrongFocus);
}

void configureStorePrimaryButton(StorePushButton *button,
    QString const& description)
{
    configureStoreButton(button, description);
    button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
    button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
    button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
    button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
    button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
    button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
    button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
}

void applyStoreTheme(QWidget *storePage)
{
    if (storePage == nullptr) {
        return;
    }
    auto mode = eTheme->getThemeMode();
    QColor panel = eTheme->getThemeColor(mode, ElaThemeType::BasicBase);
    QColor border = eTheme->getThemeColor(mode, ElaThemeType::BasicBorder);
    QColor text = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
    QColor muted = eTheme->getThemeColor(mode, ElaThemeType::BasicDetailsText);
    QColor surface = eTheme->getThemeColor(mode, ElaThemeType::WindowBase);
    QColor selected = eTheme->getThemeColor(mode, ElaThemeType::PrimaryNormal);
    QColor selectedText = eTheme->getThemeColor(mode,
        ElaThemeType::BasicTextInvert);
    QColor danger = eTheme->getThemeColor(mode, ElaThemeType::StatusDanger);
    storePage->setStyleSheet(QString(
        "#storeToolbar, #storeListSurface, #storeAccountSurface {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "#storeTitle {"
        "  color: %3;"
        "  font-weight: 600;"
        "}"
        "#storeDescription, #storeStatusLabel, #storeResultCountLabel,"
        "#storeLoginStatusLabel, #storeEmptyDescription,"
        "#storeEntryMeta, #storeEntrySummary {"
        "  color: %4;"
        "}"
        "#storeAccountTitle { color: %3; font-weight: 600; }"
        "#storeStatusBanner {"
        "  background-color: %5;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
        "#storeStatusLabel[state=\"error\"] {"
        "  color: %8; font-weight: 500;"
        "}"
        "#storeStatusBanner[state=\"error\"] { border-color: %8; }"
        "#storeStatusBanner[state=\"busy\"] { border-color: %6; }"
        "#storeToolbar QLineEdit, #storeToolbar QComboBox {"
        "  background-color: %5;"
        "  color: %3;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  min-height: 18px;"
        "}"
        "#storeToolbar QLineEdit:focus, #storeToolbar QComboBox:focus {"
        "  border: 2px solid %6;"
        "  padding: 5px 7px;"
        "}"
        "#storeList {"
        "  background-color: transparent;"
        "  color: %3;"
        "  border: none;"
        "  outline: none;"
        "}"
        "#storeList:focus {"
        "  border: 2px solid %6;"
        "  border-radius: 6px;"
        "}"
        "#storeList::item {"
        "  margin: 3px 2px;"
        "  padding: 0px;"
        "  border-radius: 7px;"
        "}"
        "#storeList::item:hover {"
        "  background-color: %5;"
        "}"
        "#storeList::item:selected {"
        "  background-color: transparent;"
        "  color: transparent;"
        "}"
        "#storeEntryCard {"
        "  background-color: %5; border: 1px solid %2; border-radius: 7px;"
        "}"
        "#storeEntryCard QLabel { color: %3; background: transparent; }"
        "#storeEntryCard[selected=\"true\"] {"
        "  background-color: %6; border: 2px solid %6;"
        "}"
        "#storeEntryCard[selected=\"true\"] QLabel { color: %7; }"
        "#storeEntryName { font-weight: 600; }"
        "#storeEmptyState { background: transparent; border: none; }"
        "#storeDownloadProgress { min-height: 6px; max-height: 6px; }"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background: %2; min-height: 24px;"
        " border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        " height: 0px; }"
    ).arg(panel.name(QColor::HexArgb), border.name(QColor::HexArgb),
        text.name(QColor::HexArgb), muted.name(QColor::HexArgb),
        surface.name(QColor::HexArgb), selected.name(QColor::HexArgb),
        selectedText.name(QColor::HexArgb), danger.name(QColor::HexArgb)));
}

QWidget *createStoreEntryCard(MascotStoreEntry const& entry, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("storeEntryCard"));
    card->setMinimumHeight(86);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    card->setAccessibleName(QStringLiteral("%1 v%2")
        .arg(entry.name, entry.version));
    card->setAccessibleDescription(entry.summary);
    card->setFocusPolicy(Qt::NoFocus);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 7, 10, 7);
    layout->setSpacing(4);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    auto *name = new QLabel(entry.name, card);
    name->setObjectName(QStringLiteral("storeEntryName"));
    auto *version = new QLabel(QStringLiteral("v%1").arg(entry.version), card);
    version->setObjectName(QStringLiteral("storeEntryMeta"));
    titleRow->addWidget(name, 1);
    titleRow->addWidget(version, 0, Qt::AlignRight | Qt::AlignTop);
    layout->addLayout(titleRow);

    auto *summary = new QLabel(entry.summary, card);
    summary->setObjectName(QStringLiteral("storeEntrySummary"));
    summary->setWordWrap(true);
    summary->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(summary);

    QStringList facets = entry.tags;
    for (auto const& category : entry.categories) {
        if (!facets.contains(category, Qt::CaseInsensitive)) {
            facets.append(category);
        }
    }
    QString meta = facets.isEmpty()
        ? createTr("Community mascot")
        : facets.join(QStringLiteral(" · "));
    if (entry.download.size > 0) {
        meta += QStringLiteral("  ·  ") +
            QLocale().formattedDataSize(entry.download.size);
    }
    auto *metaLabel = new QLabel(meta, card);
    metaLabel->setObjectName(QStringLiteral("storeEntryMeta"));
    metaLabel->setWordWrap(true);
    layout->addWidget(metaLabel);
    return card;
}

void showUserCodeDialog(QWidget *parent, QString const& userCode,
    QUrl const& verificationUrl)
{
    QDialog *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(createTr("GitHub Authorization"));
    dialog->setMinimumWidth(420);
    auto *layout = new QVBoxLayout(dialog);
    auto *hint = new QLabel(createTr(
        "Enter this code on the GitHub page that opened in your browser:"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *codeLabel = new QLabel(QStringLiteral("<h2>%1</h2>")
        .arg(userCode.toHtmlEscaped()));
    codeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(codeLabel);
    auto *urlLabel = new QLabel(verificationUrl.toDisplayString());
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    urlLabel->setWordWrap(true);
    layout->addWidget(urlLabel);
    auto *copyButton = new QPushButton(createTr("Copy code"), dialog);
    QObject::connect(copyButton, &QPushButton::clicked, dialog,
        [userCode]() {
            QGuiApplication::clipboard()->setText(userCode);
        });
    layout->addWidget(copyButton);
    auto *closeButton = new QPushButton(createTr("Close"), dialog);
    QObject::connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    dialog->show();
}

}  // namespace

void ShijimaManager::setupStorePage() {
    auto storeUi = std::make_unique<MascotStoreUi>();
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("storePage"));
    auto *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(16, 14, 16, 14);
    rootLayout->setSpacing(10);

    auto *title = new ElaText(createTr("Mascot Store"), page);
    title->setObjectName(QStringLiteral("storeTitle"));
    title->setTextPixelSize(20);
    title->setWordWrap(false);
    rootLayout->addWidget(title);

    auto *description = new QLabel(createTr(
        "Discover community mascots, review their details, and install them "
        "into your local library."), page);
    description->setObjectName(QStringLiteral("storeDescription"));
    description->setWordWrap(true);
    rootLayout->addWidget(description);

    auto *toolbarFrame = new QFrame(page);
    toolbarFrame->setObjectName(QStringLiteral("storeToolbar"));
    auto *toolbar = new QHBoxLayout(toolbarFrame);
    toolbar->setContentsMargins(10, 8, 10, 8);
    toolbar->setSpacing(8);
    storeUi->searchEdit = new QLineEdit(page);
    storeUi->searchEdit->setPlaceholderText(createTr("Search mascots..."));
    storeUi->searchEdit->setClearButtonEnabled(true);
    storeUi->searchEdit->setAccessibleName(createTr("Search mascots"));
    storeUi->searchEdit->setAccessibleDescription(createTr(
        "Filter the mascot registry by name, summary, id, or author."));
    storeUi->searchEdit->setMinimumHeight(34);
    toolbar->addWidget(storeUi->searchEdit, 1);
    storeUi->tagFilter = new QComboBox(page);
    storeUi->tagFilter->addItem(createTr("All tags"), QString {});
    storeUi->tagFilter->setAccessibleName(createTr("Filter by tag"));
    storeUi->tagFilter->setAccessibleDescription(createTr(
        "Show mascots in a selected category or tag."));
    storeUi->tagFilter->setMinimumWidth(132);
    storeUi->tagFilter->setMinimumHeight(34);
    toolbar->addWidget(storeUi->tagFilter);
    auto *refreshButton = new StorePushButton(createTr("Refresh"), page);
    storeUi->refreshButton = refreshButton;
    configureStoreButton(refreshButton, createTr(
        "Fetch the latest mascot registry."));
    toolbar->addWidget(storeUi->refreshButton);
    rootLayout->addWidget(toolbarFrame);

    auto *statusBanner = new QFrame(page);
    statusBanner->setObjectName(QStringLiteral("storeStatusBanner"));
    statusBanner->setProperty("state", QStringLiteral("info"));
    storeUi->storeStatusBanner = statusBanner;
    auto *statusRow = new QHBoxLayout(statusBanner);
    statusRow->setContentsMargins(10, 6, 10, 6);
    statusRow->setSpacing(8);
    storeUi->storeStatusLabel = new QLabel(statusBanner);
    storeUi->storeStatusLabel->setObjectName(QStringLiteral("storeStatusLabel"));
    storeUi->storeStatusLabel->setWordWrap(true);
    statusRow->addWidget(storeUi->storeStatusLabel, 1);
    storeUi->resultCountLabel = new QLabel(statusBanner);
    storeUi->resultCountLabel->setObjectName(
        QStringLiteral("storeResultCountLabel"));
    storeUi->resultCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    storeUi->resultCountLabel->setAccessibleName(createTr("Result count"));
    statusRow->addWidget(storeUi->resultCountLabel);
    rootLayout->addWidget(statusBanner);

    auto *listSurface = new QFrame(page);
    listSurface->setObjectName(QStringLiteral("storeListSurface"));
    auto *listSurfaceLayout = new QVBoxLayout(listSurface);
    listSurfaceLayout->setContentsMargins(8, 8, 8, 8);
    listSurfaceLayout->setSpacing(4);
    storeUi->entryList = new QListWidget(listSurface);
    storeUi->entryList->setObjectName(QStringLiteral("storeList"));
    storeUi->entryList->setSelectionMode(QAbstractItemView::SingleSelection);
    storeUi->entryList->setSelectionBehavior(QAbstractItemView::SelectItems);
    storeUi->entryList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    storeUi->entryList->setSpacing(1);
    storeUi->entryList->setWordWrap(true);
    storeUi->entryList->setUniformItemSizes(false);
    storeUi->entryList->setAccessibleName(createTr("Mascot registry"));
    storeUi->entryList->setAccessibleDescription(createTr(
        "Use the arrow keys to select a mascot, then choose Details or Install."));
    listSurfaceLayout->addWidget(storeUi->entryList, 1);

    auto *emptyState = new QWidget(listSurface);
    emptyState->setObjectName(QStringLiteral("storeEmptyState"));
    storeUi->emptyStateWidget = emptyState;
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(24, 28, 24, 28);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();
    storeUi->emptyStateTitleLabel = new QLabel(
        createTr("No mascots match your filters"), emptyState);
    storeUi->emptyStateTitleLabel->setObjectName(QStringLiteral("storeTitle"));
    storeUi->emptyStateTitleLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(storeUi->emptyStateTitleLabel);
    storeUi->emptyStateDescriptionLabel = new QLabel(createTr(
        "Try another search or tag, or refresh the registry."), emptyState);
    storeUi->emptyStateDescriptionLabel->setObjectName(
        QStringLiteral("storeEmptyDescription"));
    storeUi->emptyStateDescriptionLabel->setWordWrap(true);
    storeUi->emptyStateDescriptionLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(storeUi->emptyStateDescriptionLabel);
    emptyLayout->addStretch();
    listSurfaceLayout->addWidget(emptyState, 1);
    rootLayout->addWidget(listSurface, 1);

    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(2, 0, 2, 0);
    actionRow->setSpacing(8);
    storeUi->detailButton = new QPushButton(createTr("Details"), page);
    configureStoreButton(storeUi->detailButton, createTr(
        "View the selected mascot's details."));
    auto *installButton = new StorePushButton(createTr("Install"), page);
    storeUi->installButton = installButton;
    configureStorePrimaryButton(installButton, createTr(
        "Download and install the selected mascot."));
    storeUi->cancelButton = new QPushButton(createTr("Cancel download"), page);
    configureStoreButton(storeUi->cancelButton, createTr(
        "Cancel the selected mascot download."));
    actionRow->addWidget(storeUi->detailButton);
    actionRow->addWidget(storeUi->installButton);
    actionRow->addWidget(storeUi->cancelButton);
    actionRow->addStretch(1);
    rootLayout->addLayout(actionRow);

    auto *accountSurface = new QFrame(page);
    accountSurface->setObjectName(QStringLiteral("storeAccountSurface"));
    auto *accountLayout = new QVBoxLayout(accountSurface);
    accountLayout->setContentsMargins(10, 8, 10, 8);
    accountLayout->setSpacing(6);
    auto *accountHeader = new QHBoxLayout;
    accountHeader->setContentsMargins(0, 0, 0, 0);
    auto *accountTitle = new QLabel(createTr("Community submissions"),
        accountSurface);
    accountTitle->setObjectName(QStringLiteral("storeAccountTitle"));
    accountHeader->addWidget(accountTitle);
    accountHeader->addStretch(1);
    accountLayout->addLayout(accountHeader);
    auto *accountRow = new QHBoxLayout;
    accountRow->setContentsMargins(0, 0, 0, 0);
    accountRow->setSpacing(8);
    storeUi->loginButton = new QPushButton(createTr("Sign in with GitHub"),
        accountSurface);
    configureStoreButton(storeUi->loginButton, createTr(
        "Sign in to submit a mascot to the community registry."));
    storeUi->loginStatusLabel = new QLabel(accountSurface);
    storeUi->loginStatusLabel->setObjectName(
        QStringLiteral("storeLoginStatusLabel"));
    storeUi->loginStatusLabel->setWordWrap(true);
    accountRow->addWidget(storeUi->loginButton);
    accountRow->addWidget(storeUi->loginStatusLabel, 1);
    auto *submitButton = new QPushButton(createTr("Submit a mascot..."),
        accountSurface);
    storeUi->submitButton = submitButton;
    configureStoreButton(submitButton, createTr(
        "Open the mascot submission form."));
    accountRow->addWidget(submitButton);
    accountLayout->addLayout(accountRow);
    rootLayout->addWidget(accountSurface);

    storeUi->downloadProgress = new QProgressBar(page);
    storeUi->downloadProgress->setObjectName(QStringLiteral("storeDownloadProgress"));
    storeUi->downloadProgress->setTextVisible(false);
    storeUi->downloadProgress->setRange(0, 100);
    storeUi->downloadProgress->setVisible(false);
    rootLayout->addWidget(storeUi->downloadProgress);

    setTabOrder(storeUi->searchEdit, storeUi->tagFilter);
    setTabOrder(storeUi->tagFilter, storeUi->refreshButton);
    setTabOrder(storeUi->refreshButton, storeUi->entryList);
    setTabOrder(storeUi->entryList, storeUi->detailButton);
    setTabOrder(storeUi->detailButton, storeUi->installButton);
    setTabOrder(storeUi->installButton, storeUi->cancelButton);
    setTabOrder(storeUi->cancelButton, storeUi->loginButton);
    setTabOrder(storeUi->loginButton, storeUi->submitButton);

    m_ui->storeUi = std::move(storeUi);
    m_ui->storeUi->storePage = page;
    addPageNode(tr("Store"), m_ui->storeUi->storePage,
        ElaIconType::BagsShopping);

    QString dataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    m_storeCache = std::make_unique<MascotStoreCache>(
        QDir::cleanPath(dataPath + QLatin1String("/mascot-store-cache")));
    m_storeNetwork = std::make_unique<MascotStoreNetwork>(this);
    m_storeCoordinator = std::make_unique<MascotStoreCoordinator>(
        m_storeCache.get(), m_storeNetwork.get(), m_runtime->mascotsPath,
        QDir::cleanPath(dataPath + QLatin1String("/mascot-store-cache")));
    m_githubAuth = std::make_unique<GitHubAuthManager>(
        MascotStoreConfig::githubLoginClientId(),
        createPlatformCredentialStore());
    m_ui->storeUi->githubLoginConfigured =
        MascotStoreConfig::isLoginConfigured();
    if (!m_ui->storeUi->githubLoginConfigured) {
        QString unavailable = createTr(
            "GitHub login is not configured by the maintainer.");
        m_ui->storeUi->loginButton->setEnabled(false);
        m_ui->storeUi->loginButton->setToolTip(unavailable);
        m_ui->storeUi->loginButton->setAccessibleDescription(unavailable);
        m_ui->storeUi->submitButton->setEnabled(false);
        m_ui->storeUi->loginStatusLabel->setText(unavailable);
    }
    m_submissionClient = std::make_unique<MascotSubmissionClient>(
        QUrl { MascotStoreConfig::submissionServiceUrl() });

    auto setStoreStatus = [this](QString const& text, QString const& state) {
        auto *ui = m_ui->storeUi.get();
        ui->storeStatusLabel->setText(text);
        ui->storeStatusLabel->setProperty("state", state);
        ui->storeStatusBanner->setProperty("state", state);
        ui->storeStatusLabel->style()->unpolish(ui->storeStatusLabel);
        ui->storeStatusLabel->style()->polish(ui->storeStatusLabel);
        ui->storeStatusBanner->style()->unpolish(ui->storeStatusBanner);
        ui->storeStatusBanner->style()->polish(ui->storeStatusBanner);
        ui->storeStatusBanner->update();
    };

    auto updateEntryActions = [this]() {
        auto *ui = m_ui->storeUi.get();
        QListWidgetItem *item = ui->entryList->currentItem();
        QString selectedId = item == nullptr
            ? QString {} : item->data(kMascotIdRole).toString();
        bool hasSelection = !selectedId.isEmpty() &&
            m_lastStoreIndex.findById(selectedId) != nullptr;
        bool anyBusy = m_storeCoordinator != nullptr &&
            m_storeCoordinator->hasActiveOperation();
        bool downloading = hasSelection &&
            m_storeCoordinator->isDownloading(selectedId);
        ui->detailButton->setEnabled(hasSelection);
        ui->installButton->setEnabled(hasSelection && !anyBusy);
        ui->cancelButton->setEnabled(downloading);
        ui->refreshButton->setEnabled(!ui->indexRefreshing && !anyBusy);
    };

    auto refreshList = [this, updateEntryActions]() {
        auto *ui = m_ui->storeUi.get();
        QListWidget *list = ui->entryList;
        QString previousSelection;
        if (auto *item = list->currentItem()) {
            previousSelection = item->data(kMascotIdRole).toString();
        }
        {
            QSignalBlocker blocker(list);
            list->clear();
            QString query = ui->searchEdit->text();
            QString tag = ui->tagFilter->currentData().toString();
            auto entries = m_lastStoreIndex.filter(query,
                tag.isEmpty() ? QStringList {} : QStringList { tag });
            int restoredRow = -1;
            for (auto const& entry : entries) {
                auto *item = new QListWidgetItem(list);
                // The card widget renders the visible content. Keeping the
                // item's display text empty prevents QListWidget from drawing
                // a second copy underneath the selected card; the card's
                // accessible name/description remains available to assistive
                // technologies.
                item->setText(QString {});
                item->setData(kMascotIdRole, entry.id);
                item->setToolTip(entry.summary);
                auto *card = createStoreEntryCard(entry, list);
                list->setItemWidget(item, card);
                if (entry.id == previousSelection) {
                    restoredRow = list->row(item);
                }
            }
            if (restoredRow >= 0) {
                list->setCurrentRow(restoredRow);
            }
        }
        bool hasEntries = list->count() > 0;
        list->setVisible(hasEntries);
        ui->emptyStateWidget->setVisible(!hasEntries);
        int resultCount = list->count();
        ui->resultCountLabel->setText(createTr("%1 mascots")
            .arg(resultCount));
        for (int row = 0; row < list->count(); ++row) {
            setStoreCardSelected(list->item(row),
                list->item(row) == list->currentItem());
        }
        updateEntryActions();
    };

    auto refreshTags = [this, refreshList]() {
        QComboBox *box = m_ui->storeUi->tagFilter;
        QString current = box->currentData().toString();
        {
            QSignalBlocker blocker(box);
            box->clear();
            box->addItem(createTr("All tags"), QString {});
            QSet<QString> tags;
            for (auto const& entry : m_lastStoreIndex.entries) {
                tags.unite(QSet<QString>(entry.tags.begin(), entry.tags.end()));
                tags.unite(QSet<QString>(entry.categories.begin(),
                    entry.categories.end()));
            }
            QStringList sorted(tags.begin(), tags.end());
            sorted.sort(Qt::CaseInsensitive);
            for (auto const& tag : sorted) {
                box->addItem(tag, tag);
            }
            int index = box->findData(current);
            box->setCurrentIndex(index >= 0 ? index : 0);
        }
        refreshList();
    };

    connect(m_ui->storeUi->refreshButton, &QPushButton::clicked, this,
        [this, setStoreStatus, updateEntryActions]() {
            auto *ui = m_ui->storeUi.get();
            // Set the busy state before entering the coordinator. Invalid or
            // unconfigured URLs report synchronously and must not be replaced
            // by a stale “Refreshing...” message afterwards.
            ui->indexRefreshing = true;
            setStoreStatus(createTr("Refreshing store..."),
                QStringLiteral("busy"));
            updateEntryActions();
            m_storeCoordinator->refreshIndex();
        });
    connect(m_ui->storeUi->searchEdit, &QLineEdit::textChanged, this,
        [refreshList](QString const&) { refreshList(); });
    connect(m_ui->storeUi->tagFilter, &QComboBox::currentIndexChanged, this,
        [refreshList](int) { refreshList(); });
    connect(m_ui->storeUi->entryList, &QListWidget::itemSelectionChanged, this,
        [this, updateEntryActions]() {
            auto *list = m_ui->storeUi->entryList;
            for (int row = 0; row < list->count(); ++row) {
                setStoreCardSelected(list->item(row),
                    list->item(row) == list->currentItem());
            }
            updateEntryActions();
        });

    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::indexStateChanged,
        this, [this, refreshList, refreshTags, setStoreStatus,
            updateEntryActions](
            MascotStoreCoordinator::IndexState state) {
            auto *ui = m_ui->storeUi.get();
            ui->indexRefreshing = false;
            if (state.loaded) {
                m_lastStoreIndex = state.index;
                refreshTags();
                setStoreStatus(
                    state.fromCache
                        ? (state.stale
                            ? createTr("Offline: showing the last cached index.")
                            : createTr("Loaded from the local cache."))
                        : createTr("Loaded %1 mascots from the registry.")
                            .arg(state.index.entries.size()),
                    state.stale ? QStringLiteral("warning")
                                : QStringLiteral("success"));
            }
            else {
                if (state.errorCode ==
                    QStringLiteral("mascotstore.not_configured")) {
                    // A cache from a different (for example staging) profile
                    // must not masquerade as the current unconfigured store.
                    m_lastStoreIndex = MascotStoreIndex {};
                    refreshTags();
                }
                else {
                    refreshList();
                }
                QString error;
                if (state.errorCode ==
                    QStringLiteral("mascotstore.not_configured")) {
                    error = createTr(
                        "The mascot store is not configured by the maintainer.");
                }
                else {
                    error = state.error.isEmpty()
                        ? createTr("The registry request failed.") : state.error;
                }
                setStoreStatus(createTr("Store unavailable: %1").arg(error),
                    QStringLiteral("error"));
            }
            updateEntryActions();
        });
    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::entryProgress,
        this, [this, setStoreStatus, updateEntryActions](QString id, qint64 received,
            qint64 total) {
            auto *ui = m_ui->storeUi.get();
            if (total > 0) {
                ui->downloadProgress->setRange(0,
                    total > INT_MAX ? 0 : static_cast<int>(total));
                ui->downloadProgress->setValue(
                    total > INT_MAX ? 0 : static_cast<int>(received));
                ui->downloadProgress->setVisible(true);
                setStoreStatus(
                    createTr("Downloading %1... %2 / %3")
                        .arg(id,
                            QLocale().formattedDataSize(received),
                            QLocale().formattedDataSize(total)),
                    QStringLiteral("busy"));
            }
            else {
                // Servers may omit Content-Length. Keep the operation visible
                // with an indeterminate bar instead of leaving stale status.
                ui->downloadProgress->setRange(0, 0);
                ui->downloadProgress->setVisible(true);
                setStoreStatus(createTr("Downloading %1...").arg(id),
                    QStringLiteral("busy"));
            }
            updateEntryActions();
        });
    connect(m_storeCoordinator.get(),
        &MascotStoreCoordinator::entryInstallStarted, this,
        [this, setStoreStatus, updateEntryActions](QString id) {
            auto *ui = m_ui->storeUi.get();
            ui->downloadProgress->setRange(0, 0);
            ui->downloadProgress->setVisible(true);
            setStoreStatus(createTr("Installing %1...").arg(id),
                QStringLiteral("busy"));
            updateEntryActions();
        });
    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::entryFinished,
        this, [this, setStoreStatus, updateEntryActions](QString id, bool ok,
            QString installedName, QString errorCode, QString error) {
            auto *ui = m_ui->storeUi.get();
            ui->downloadProgress->setVisible(false);
            if (ok) {
                setStoreStatus(createTr("Installed %1.").arg(installedName),
                    QStringLiteral("success"));
                reloadMascots({ installedName.toStdString() });
            }
            else if (errorCode == QStringLiteral("mascotstore.download.canceled")) {
                setStoreStatus(createTr("Download canceled."),
                    QStringLiteral("info"));
            }
            else {
                setStoreStatus(
                    createTr("Install failed (%1): %2")
                        .arg(errorCode, error), QStringLiteral("error"));
            }
            Q_UNUSED(id);
            updateEntryActions();
        });

    auto selectedEntry = [this]() -> MascotStoreEntry const* {
        QListWidgetItem *item = m_ui->storeUi->entryList->currentItem();
        if (item == nullptr) {
            return nullptr;
        }
        return m_lastStoreIndex.findById(
            item->data(kMascotIdRole).toString());
    };
    connect(m_ui->storeUi->installButton, &QPushButton::clicked, this,
        [this, selectedEntry, setStoreStatus, updateEntryActions]() {
            if (auto const* entry = selectedEntry()) {
                setStoreStatus(createTr("Preparing %1...").arg(entry->name),
                    QStringLiteral("busy"));
                updateEntryActions();
                m_storeCoordinator->downloadAndInstall(*entry);
            }
        });
    connect(m_ui->storeUi->cancelButton, &QPushButton::clicked, this,
        [this, selectedEntry, updateEntryActions]() {
            if (auto const* entry = selectedEntry()) {
                m_storeCoordinator->cancelDownload(entry->id);
                updateEntryActions();
            }
        });
    connect(m_ui->storeUi->detailButton, &QPushButton::clicked, this,
        [this, selectedEntry]() {
            if (auto const* entry = selectedEntry()) {
                showMascotStoreDetail(entry);
            }
        });
    connect(m_ui->storeUi->entryList, &QListWidget::itemDoubleClicked, this,
        [this, selectedEntry](QListWidgetItem *) {
            if (auto const* entry = selectedEntry()) {
                showMascotStoreDetail(entry);
            }
        });

    connect(m_githubAuth.get(), &GitHubAuthManager::stateChanged, this,
        [this]() {
            if (m_githubAuth->isSignedIn()) {
                m_ui->storeUi->loginButton->setText(createTr("Sign out"));
                m_ui->storeUi->submitButton->setEnabled(true);
                QString status = createTr("Signed in as %1")
                    .arg(m_githubAuth->userInfo().login);
                if (!m_githubAuth->canPersistLogin()) {
                    status += QStringLiteral("  ") +
                        createTr("(this session only; secure persistence is "
                                 "not available on this platform)");
                }
                m_ui->storeUi->loginStatusLabel->setText(status);
            }
            else {
                m_ui->storeUi->loginButton->setText(
                    createTr("Sign in with GitHub"));
                m_ui->storeUi->submitButton->setEnabled(
                    m_ui->storeUi->githubLoginConfigured);
                if (!m_ui->storeUi->githubLoginConfigured) {
                    m_ui->storeUi->loginStatusLabel->setText(createTr(
                        "GitHub login is not configured by the maintainer."));
                }
                else {
                    m_ui->storeUi->loginStatusLabel->setText(
                        createTr("Not signed in."));
                }
            }
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::deviceCodeReady, this,
        [this](QString userCode, QUrl verificationUrl) {
            showUserCodeDialog(this, userCode, verificationUrl);
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::signedOut, this,
        [this, setStoreStatus]() {
            setStoreStatus(createTr("Signed out of GitHub."),
                QStringLiteral("info"));
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::errorOccurred, this,
        [this, setStoreStatus](QString code, QString message) {
            QString status = createTr("GitHub error (%1): %2")
                .arg(code, redactSensitiveText(message));
            setStoreStatus(status, QStringLiteral("error"));
            m_ui->storeUi->loginStatusLabel->setText(status);
        });
    connect(m_ui->storeUi->loginButton, &QPushButton::clicked, this,
        [this]() {
            if (m_githubAuth->isSignedIn()) {
                m_githubAuth->signOut();
            }
            else {
                m_githubAuth->startDeviceFlow();
            }
        });
    connect(m_ui->storeUi->submitButton, &QPushButton::clicked, this,
        [this]() {
            if (!m_githubAuth->isSignedIn()) {
                m_githubAuth->startDeviceFlow();
                m_ui->storeUi->storeStatusLabel->setText(
                    createTr("Sign in with GitHub before submitting."));
                return;
            }
            auto *dialog = new MascotSubmissionDialog(
                m_githubAuth.get(), m_submissionClient.get(), this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        });

    applyStoreTheme(page);
    // Start with the cached index only when an index source is configured.
    // Otherwise an old staging cache must not appear beside the unavailable
    // production Store banner.
    if (MascotStoreConfig::isIndexConfigured()) {
        m_storeCoordinator->loadCachedIndex();
    }
    else {
        m_lastStoreIndex = MascotStoreIndex {};
        refreshTags();
        setStoreStatus(createTr(
            "Store unavailable: The mascot store is not configured by the maintainer"),
            QStringLiteral("error"));
    }
    connect(eTheme, &ElaTheme::themeModeChanged, page, [page]() {
        applyStoreTheme(page);
    });
    updateEntryActions();
}

void ShijimaManager::showMascotStoreDetail(MascotStoreEntry const* entry) {
    if (entry == nullptr) {
        return;
    }
    auto *dialog = new MascotStoreDetailDialog(*entry, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
