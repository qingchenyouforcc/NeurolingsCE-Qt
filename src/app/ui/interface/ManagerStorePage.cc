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
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTextBrowser>

#include "ElaIcon.h"

namespace {

int const kMascotIdRole = Qt::UserRole;

QString createTr(char const *sourceText) {
    return QCoreApplication::translate("ShijamaManager", sourceText);
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
    auto *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(8);

    auto *toolbar = new QHBoxLayout;
    storeUi->searchEdit = new QLineEdit(page);
    storeUi->searchEdit->setPlaceholderText(createTr("Search mascots..."));
    toolbar->addWidget(storeUi->searchEdit, 1);
    storeUi->tagFilter = new QComboBox(page);
    storeUi->tagFilter->addItem(createTr("All tags"), QString {});
    toolbar->addWidget(storeUi->tagFilter);
    storeUi->refreshButton = new QPushButton(createTr("Refresh"), page);
    toolbar->addWidget(storeUi->refreshButton);
    rootLayout->addLayout(toolbar);

    storeUi->storeStatusLabel = new QLabel(page);
    storeUi->storeStatusLabel->setWordWrap(true);
    rootLayout->addWidget(storeUi->storeStatusLabel);

    storeUi->entryList = new QListWidget(page);
    rootLayout->addWidget(storeUi->entryList, 1);

    auto *actionRow = new QHBoxLayout;
    storeUi->detailButton = new QPushButton(createTr("Details"), page);
    storeUi->installButton = new QPushButton(createTr("Install"), page);
    storeUi->cancelButton = new QPushButton(createTr("Cancel download"), page);
    storeUi->submitButton = new QPushButton(createTr("Submit a mascot..."), page);
    actionRow->addWidget(storeUi->detailButton);
    actionRow->addWidget(storeUi->installButton);
    actionRow->addWidget(storeUi->cancelButton);
    actionRow->addStretch(1);
    actionRow->addWidget(storeUi->submitButton);
    rootLayout->addLayout(actionRow);

    auto *loginRow = new QHBoxLayout;
    storeUi->loginButton = new QPushButton(createTr("Sign in with GitHub"), page);
    storeUi->loginStatusLabel = new QLabel(page);
    loginRow->addWidget(storeUi->loginButton);
    loginRow->addWidget(storeUi->loginStatusLabel, 1);
    rootLayout->addLayout(loginRow);

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
    m_submissionClient = std::make_unique<MascotSubmissionClient>(
        QUrl { MascotStoreConfig::submissionServiceUrl() });

    auto refreshList = [this]() {
        QListWidget *list = m_ui->storeUi->entryList;
        list->clear();
        MascotStoreIndex const& index = m_lastStoreIndex;
        QString query = m_ui->storeUi->searchEdit->text();
        QString tag = m_ui->storeUi->tagFilter->currentData().toString();
        auto entries = index.filter(query,
            tag.isEmpty() ? QStringList {} : QStringList { tag });
        for (auto const& entry : entries) {
            auto *item = new QListWidgetItem(
                QStringLiteral("%1  v%2\n%3")
                    .arg(entry.name, entry.version, entry.summary),
                list);
            item->setData(kMascotIdRole, entry.id);
            item->setToolTip(entry.summary);
        }
    };

    auto refreshTags = [this]() {
        QComboBox *box = m_ui->storeUi->tagFilter;
        QString current = box->currentData().toString();
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
        if (index >= 0) {
            box->setCurrentIndex(index);
        }
    };

    connect(m_ui->storeUi->refreshButton, &QPushButton::clicked, this,
        [this]() {
            m_storeCoordinator->refreshIndex();
            m_ui->storeUi->storeStatusLabel->setText(createTr("Refreshing..."));
        });
    connect(m_ui->storeUi->searchEdit, &QLineEdit::textChanged, this,
        [refreshList](QString const&) { refreshList(); });
    connect(m_ui->storeUi->tagFilter, &QComboBox::currentIndexChanged, this,
        [refreshList](int) { refreshList(); });

    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::indexStateChanged,
        this, [this, refreshList, refreshTags](
            MascotStoreCoordinator::IndexState state) {
            if (state.loaded) {
                m_lastStoreIndex = state.index;
                refreshTags();
                refreshList();
                m_ui->storeUi->storeStatusLabel->setText(
                    state.fromCache
                        ? (state.stale
                            ? createTr("Offline: showing the last cached index.")
                            : createTr("Loaded from the local cache."))
                        : createTr("Loaded %1 mascots from the registry.")
                            .arg(state.index.entries.size()));
            }
            else {
                m_ui->storeUi->storeStatusLabel->setText(
                    createTr("Store unavailable: %1").arg(state.error));
            }
        });
    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::entryProgress,
        this, [this](QString id, qint64 received, qint64 total) {
            Q_UNUSED(id);
            if (total > 0) {
                m_ui->storeUi->storeStatusLabel->setText(
                    createTr("Downloading... %1 / %2")
                        .arg(QLocale().formattedDataSize(received),
                             QLocale().formattedDataSize(total)));
            }
        });
    connect(m_storeCoordinator.get(), &MascotStoreCoordinator::entryFinished,
        this, [this](QString id, bool ok, QString installedName,
            QString errorCode, QString error) {
            if (ok) {
                m_ui->storeUi->storeStatusLabel->setText(
                    createTr("Installed %1.").arg(installedName));
                reloadMascots({ installedName.toStdString() });
            }
            else {
                m_ui->storeUi->storeStatusLabel->setText(
                    createTr("Install failed (%1): %2")
                        .arg(errorCode, error));
            }
            Q_UNUSED(id);
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
        [this, selectedEntry]() {
            if (auto const* entry = selectedEntry()) {
                m_storeCoordinator->downloadAndInstall(*entry);
            }
        });
    connect(m_ui->storeUi->cancelButton, &QPushButton::clicked, this,
        [this, selectedEntry]() {
            if (auto const* entry = selectedEntry()) {
                m_storeCoordinator->cancelDownload(entry->id);
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
                m_ui->storeUi->loginStatusLabel->clear();
            }
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::deviceCodeReady, this,
        [this](QString userCode, QUrl verificationUrl) {
            showUserCodeDialog(this, userCode, verificationUrl);
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::signedOut, this,
        [this]() {
            m_ui->storeUi->storeStatusLabel->setText(
                createTr("Signed out of GitHub."));
        });
    connect(m_githubAuth.get(), &GitHubAuthManager::errorOccurred, this,
        [this](QString code, QString message) {
            m_ui->storeUi->storeStatusLabel->setText(
                createTr("GitHub error (%1): %2")
                    .arg(code, redactSensitiveText(message)));
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

    // Start with the cached index so the page is usable offline.
    m_storeCoordinator->loadCachedIndex();
}

void ShijimaManager::showMascotStoreDetail(MascotStoreEntry const* entry) {
    if (entry == nullptr) {
        return;
    }
    auto *dialog = new MascotStoreDetailDialog(*entry, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
