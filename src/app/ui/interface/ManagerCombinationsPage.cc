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
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"
#include "../../runtime/ManagerRuntimeState.hpp"
#include "../ManagerUiState.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QVBoxLayout>

#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"

namespace {

constexpr char kSavedCombinationsKey[] = "combinations/saved";
constexpr char kLastCombinationKey[] = "combinations/lastBeforeClose";
constexpr int kCombinationTypeRole = Qt::UserRole;
constexpr int kCombinationIdRole = Qt::UserRole + 1;
constexpr int kCombinationPayloadRole = Qt::UserRole + 2;
constexpr int kCombinationIsRestorableRole = Qt::UserRole + 3;

enum class CombinationType {
    LastBeforeClose = 0,
    Saved = 1,
};

QString combinationTr(char const *sourceText)
{
    return QCoreApplication::translate("ShijimaManager", sourceText);
}

QString formatSavedAt(QString const& isoDate)
{
    QDateTime savedAt = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!savedAt.isValid()) {
        return combinationTr("Not saved yet");
    }
    return savedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QJsonObject parseCombination(QString const& payload)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

QString combinationToPayload(QJsonObject const& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject currentCombinationObject(ShijimaManagerRuntimeState const& runtime)
{
    QMap<QString, int> counts;
    for (auto mascot : runtime.sessions.mascots()) {
        if (mascot == nullptr || !mascot->isVisible()) {
            continue;
        }
        counts[mascot->mascotName()] += 1;
    }

    QJsonArray mascots;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        mascots.append(QJsonObject {
            { QStringLiteral("name"), it.key() },
            { QStringLiteral("count"), it.value() },
        });
    }

    return QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("mascots"), mascots },
    };
}

QJsonArray savedCombinationArray(QSettings const& settings)
{
    QString payload = settings.value(kSavedCombinationsKey).toString();
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }
    return document.array();
}

QJsonObject savedCombinationById(QSettings const& settings, QString const& id)
{
    if (id.isEmpty()) {
        return {};
    }
    for (auto const& value : savedCombinationArray(settings)) {
        QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() == id) {
            return entry.value(QStringLiteral("combination")).toObject();
        }
    }
    return {};
}

void writeSavedCombinationArray(QSettings& settings, QJsonArray const& array)
{
    settings.setValue(kSavedCombinationsKey,
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

int totalMascotCount(QJsonObject const& combination)
{
    int total = 0;
    for (auto const& value : combination.value(QStringLiteral("mascots")).toArray()) {
        total += value.toObject().value(QStringLiteral("count")).toInt();
    }
    return total;
}

QString combinationSummary(QJsonObject const& combination)
{
    QJsonArray mascots = combination.value(QStringLiteral("mascots")).toArray();
    if (mascots.isEmpty()) {
        return combinationTr("No mascots in this combination.");
    }

    QStringList pieces;
    int shown = 0;
    for (auto const& value : mascots) {
        QJsonObject item = value.toObject();
        QString name = item.value(QStringLiteral("name")).toString();
        int count = item.value(QStringLiteral("count")).toInt();
        if (name.isEmpty() || count <= 0) {
            continue;
        }
        if (shown < 3) {
            pieces.append(combinationTr("%1 x%2").arg(name).arg(count));
        }
        ++shown;
    }
    if (shown > 3) {
        pieces.append(combinationTr("and %1 more").arg(shown - 3));
    }
    return pieces.join(QStringLiteral(", "));
}

QString combinationDetails(QString const& title, QJsonObject const& combination)
{
    QStringList lines;
    lines.append(title);
    lines.append(combinationTr("Saved at: %1")
        .arg(formatSavedAt(combination.value(QStringLiteral("savedAt")).toString())));
    lines.append(combinationTr("Total mascots: %1").arg(totalMascotCount(combination)));
    lines.append(QString());

    QJsonArray mascots = combination.value(QStringLiteral("mascots")).toArray();
    if (mascots.isEmpty()) {
        lines.append(combinationTr("No mascots in this combination."));
        return lines.join(QStringLiteral("\n"));
    }

    for (auto const& value : mascots) {
        QJsonObject item = value.toObject();
        QString name = item.value(QStringLiteral("name")).toString();
        int count = item.value(QStringLiteral("count")).toInt();
        if (!name.isEmpty() && count > 0) {
            lines.append(combinationTr("%1 x%2").arg(name).arg(count));
        }
    }
    return lines.join(QStringLiteral("\n"));
}

void configureCombinationButton(ElaPushButton *button)
{
    button->setMinimumHeight(34);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

QFrame *makeCombinationPanel(QWidget *parent)
{
    auto mode = eTheme->getThemeMode();
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("CombinationPanel"));
    panel->setStyleSheet(QString(
        "#CombinationPanel {"
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

}

void ShijimaManager::setupCombinationsPage()
{
    m_ui->combinationsPage = new QWidget(this);
    auto *root = new QVBoxLayout(m_ui->combinationsPage);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    auto *title = new ElaText(tr("Combinations"), m_ui->combinationsPage);
    title->setTextPixelSize(20);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("#ElaText { background-color: transparent; border: none; }"));
    root->addWidget(title);

    auto *description = new QLabel(
        tr("Save the mascots currently on your desktop and restore the same mix later."),
        m_ui->combinationsPage);
    description->setWordWrap(true);
    description->setProperty("muted", true);
    root->addWidget(description);

    auto *actionPanel = makeCombinationPanel(m_ui->combinationsPage);
    auto *actionLayout = new QHBoxLayout(actionPanel);
    actionLayout->setContentsMargins(14, 10, 14, 10);
    actionLayout->setSpacing(8);

    auto *saveButton = new ElaPushButton(tr("Save Current Combination"), actionPanel);
    saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    configureCombinationButton(saveButton);
    connect(saveButton, &ElaPushButton::clicked, this, &ShijimaManager::saveCurrentCombination);
    actionLayout->addWidget(saveButton);

    auto *refreshButton = new ElaPushButton(tr("Refresh"), actionPanel);
    refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    configureCombinationButton(refreshButton);
    connect(refreshButton, &ElaPushButton::clicked, this, &ShijimaManager::refreshCombinationPage);
    actionLayout->addWidget(refreshButton);

    actionLayout->addStretch();
    root->addWidget(actionPanel);

    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(10);

    auto *listPanel = makeCombinationPanel(m_ui->combinationsPage);
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(14, 12, 14, 12);
    listLayout->setSpacing(8);

    auto *listTitle = new QLabel(tr("Saved Combinations"), listPanel);
    listTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    listLayout->addWidget(listTitle);

    m_ui->combinationListWidget = new QListWidget(listPanel);
    m_ui->combinationListWidget->setSelectionMode(QListWidget::SingleSelection);
    m_ui->combinationListWidget->setUniformItemSizes(false);
    listLayout->addWidget(m_ui->combinationListWidget, 1);
    contentRow->addWidget(listPanel, 2);

    auto *detailsPanel = makeCombinationPanel(m_ui->combinationsPage);
    detailsPanel->setMinimumWidth(260);
    detailsPanel->setMaximumWidth(340);
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(14, 12, 14, 12);
    detailsLayout->setSpacing(8);

    auto *detailsTitle = new QLabel(tr("Details"), detailsPanel);
    detailsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    detailsLayout->addWidget(detailsTitle);

    m_ui->combinationDetailsLabel = new QLabel(tr("Select a combination."), detailsPanel);
    m_ui->combinationDetailsLabel->setWordWrap(true);
    m_ui->combinationDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ui->combinationDetailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_ui->combinationDetailsLabel->setProperty("muted", true);
    detailsLayout->addWidget(m_ui->combinationDetailsLabel, 1);

    m_ui->restoreCombinationButton = new ElaPushButton(tr("Restore Combination"), detailsPanel);
    m_ui->restoreCombinationButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    configureCombinationButton(static_cast<ElaPushButton *>(m_ui->restoreCombinationButton));
    connect(m_ui->restoreCombinationButton, &QPushButton::clicked,
        this, &ShijimaManager::restoreSelectedCombination);
    detailsLayout->addWidget(m_ui->restoreCombinationButton);

    m_ui->deleteCombinationButton = new ElaPushButton(tr("Delete Saved Combination"), detailsPanel);
    m_ui->deleteCombinationButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    configureCombinationButton(static_cast<ElaPushButton *>(m_ui->deleteCombinationButton));
    connect(m_ui->deleteCombinationButton, &QPushButton::clicked,
        this, &ShijimaManager::deleteSelectedCombination);
    detailsLayout->addWidget(m_ui->deleteCombinationButton);

    contentRow->addWidget(detailsPanel);
    root->addLayout(contentRow, 1);

    connect(m_ui->combinationListWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        auto *item = m_ui->combinationListWidget->currentItem();
        bool hasItem = item != nullptr;
        bool restorable = hasItem && item->data(kCombinationIsRestorableRole).toBool();
        bool canDelete = hasItem && item->data(kCombinationTypeRole).toInt() ==
            static_cast<int>(CombinationType::Saved);

        if (m_ui->restoreCombinationButton != nullptr) {
            m_ui->restoreCombinationButton->setEnabled(restorable);
        }
        if (m_ui->deleteCombinationButton != nullptr) {
            m_ui->deleteCombinationButton->setEnabled(canDelete);
        }
        if (m_ui->combinationDetailsLabel != nullptr) {
            if (!hasItem) {
                m_ui->combinationDetailsLabel->setText(tr("Select a combination."));
            }
            else {
                QJsonObject combination = parseCombination(
                    item->data(kCombinationPayloadRole).toString());
                m_ui->combinationDetailsLabel->setText(
                    combinationDetails(item->text().section(QLatin1Char('\n'), 0, 0),
                        combination));
            }
        }
    });

    refreshCombinationPage();
    addPageNode(tr("Combinations"), m_ui->combinationsPage, ElaIconType::ObjectGroup);
}

void ShijimaManager::refreshCombinationPage()
{
    if (m_ui->combinationListWidget == nullptr) {
        return;
    }

    m_ui->combinationListWidget->clear();

    QJsonObject lastCombination = parseCombination(
        m_settings->value(kLastCombinationKey).toString());
    QString lastPayload = combinationToPayload(lastCombination);
    auto *lastItem = new QListWidgetItem;
    lastItem->setText(tr("Last Combination Before Close") + QStringLiteral("\n") +
        combinationSummary(lastCombination));
    lastItem->setData(kCombinationTypeRole,
        static_cast<int>(CombinationType::LastBeforeClose));
    lastItem->setData(kCombinationIdRole, QStringLiteral("lastBeforeClose"));
    lastItem->setData(kCombinationPayloadRole, lastPayload);
    lastItem->setData(kCombinationIsRestorableRole, totalMascotCount(lastCombination) > 0);
    lastItem->setSizeHint(QSize(0, 58));
    m_ui->combinationListWidget->addItem(lastItem);

    QJsonArray saved = savedCombinationArray(*m_settings);
    for (auto const& value : saved) {
        QJsonObject entry = value.toObject();
        QJsonObject combination = entry.value(QStringLiteral("combination")).toObject();
        QString name = entry.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = tr("Untitled Combination");
        }

        auto *item = new QListWidgetItem;
        item->setText(name + QStringLiteral("\n") + combinationSummary(combination));
        item->setData(kCombinationTypeRole, static_cast<int>(CombinationType::Saved));
        item->setData(kCombinationIdRole, entry.value(QStringLiteral("id")).toString());
        item->setData(kCombinationPayloadRole, combinationToPayload(combination));
        item->setData(kCombinationIsRestorableRole, totalMascotCount(combination) > 0);
        item->setSizeHint(QSize(0, 58));
        m_ui->combinationListWidget->addItem(item);
    }

    if (m_ui->restoreCombinationButton != nullptr) {
        m_ui->restoreCombinationButton->setEnabled(false);
    }
    if (m_ui->deleteCombinationButton != nullptr) {
        m_ui->deleteCombinationButton->setEnabled(false);
    }
    if (m_ui->combinationDetailsLabel != nullptr) {
        m_ui->combinationDetailsLabel->setText(tr("Select a combination."));
    }
}

void ShijimaManager::saveCurrentCombination()
{
    QJsonObject combination = currentCombinationObject(*m_runtime);
    if (totalMascotCount(combination) == 0) {
        QMessageBox::information(this,
            tr("Combinations"),
            tr("There are no active mascots to save."));
        return;
    }

    QString defaultName = tr("Combination %1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    bool ok = false;
    QString name = QInputDialog::getText(this,
        tr("Save Combination"),
        tr("Combination name:"),
        QLineEdit::Normal,
        defaultName,
        &ok).trimmed();
    if (!ok) {
        return;
    }
    if (name.isEmpty()) {
        name = defaultName;
    }

    QJsonArray saved = savedCombinationArray(*m_settings);
    saved.append(QJsonObject {
        { QStringLiteral("id"), QString::number(QDateTime::currentMSecsSinceEpoch()) },
        { QStringLiteral("name"), name },
        { QStringLiteral("combination"), combination },
    });
    writeSavedCombinationArray(*m_settings, saved);
    refreshCombinationPage();
}

void ShijimaManager::restoreSelectedCombination()
{
    if (m_ui->combinationListWidget == nullptr) {
        return;
    }
    auto *item = m_ui->combinationListWidget->currentItem();
    if (item == nullptr) {
        return;
    }

    QJsonObject combination = parseCombination(item->data(kCombinationPayloadRole).toString());
    if (totalMascotCount(combination) == 0) {
        QMessageBox::information(this,
            tr("Combinations"),
            tr("This combination does not contain any mascots."));
        return;
    }

    restoreCombination(combination, true);
}

int ShijimaManager::restoreCombination(QJsonObject const& combination, bool showMessages)
{
    if (totalMascotCount(combination) == 0) {
        return 0;
    }

    killAll();
    QStringList missing;
    int restored = 0;
    for (auto const& value : combination.value(QStringLiteral("mascots")).toArray()) {
        QJsonObject savedMascot = value.toObject();
        QString name = savedMascot.value(QStringLiteral("name")).toString();
        int count = savedMascot.value(QStringLiteral("count")).toInt();
        if (name.isEmpty() || count <= 0) {
            continue;
        }
        if (!m_runtime->templates.loadedMascots().contains(name)) {
            missing.append(name);
            continue;
        }
        for (int i = 0; i < count; ++i) {
            if (spawn(name.toStdString()) != nullptr) {
                ++restored;
            }
        }
    }

    updateStatusBar();
    if (showMessages && !missing.isEmpty()) {
        missing.removeDuplicates();
        QMessageBox::warning(this,
            tr("Combinations"),
            tr("Restored %1 mascot(s). Missing templates: %2")
                .arg(restored)
                .arg(missing.join(QStringLiteral(", "))));
    }
    return restored;
}

void ShijimaManager::restoreStartupCombination()
{
    if (m_settings == nullptr) {
        return;
    }

    QString mode = m_settings->value(QStringLiteral("startup/restoreCombinationMode"),
        QStringLiteral("last")).toString();
    QJsonObject combination;
    if (mode == QStringLiteral("last")) {
        combination = parseCombination(m_settings->value(kLastCombinationKey).toString());
    }
    else if (mode == QStringLiteral("saved")) {
        combination = savedCombinationById(*m_settings,
            m_settings->value(QStringLiteral("startup/restoreCombinationId")).toString());
    }
    else {
        return;
    }

    restoreCombination(combination, false);
}

void ShijimaManager::deleteSelectedCombination()
{
    if (m_ui->combinationListWidget == nullptr) {
        return;
    }
    auto *item = m_ui->combinationListWidget->currentItem();
    if (item == nullptr || item->data(kCombinationTypeRole).toInt() !=
        static_cast<int>(CombinationType::Saved))
    {
        return;
    }

    QString id = item->data(kCombinationIdRole).toString();
    if (id.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this,
        tr("Delete Combination"),
        tr("Delete this saved combination?")) != QMessageBox::Yes)
    {
        return;
    }

    QJsonArray saved = savedCombinationArray(*m_settings);
    QJsonArray updated;
    for (auto const& value : saved) {
        QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() != id) {
            updated.append(entry);
        }
    }
    writeSavedCombinationArray(*m_settings, updated);
    refreshCombinationPage();
}

void ShijimaManager::saveLastCombinationBeforeShutdown()
{
    if (m_settings == nullptr || m_runtime == nullptr) {
        return;
    }
    m_settings->setValue(kLastCombinationKey,
        combinationToPayload(currentCombinationObject(*m_runtime)));
    m_settings->sync();
}
