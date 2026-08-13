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
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"
#include "../../runtime/ManagerRuntimeState.hpp"
#include "../ManagerUiState.hpp"
#include "../ManagerUiHelpers.hpp"

#include <exception>

#include <QCoreApplication>
#include <QDateTime>
#include <QBoxLayout>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QLocale>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
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
constexpr int kMaxMascotsPerEntry = 50;
constexpr int kMaxMascotsPerCombination = 200;
constexpr int kCompactCombinationWidth = 720;
constexpr int kCompactCombinationActionWidth = 520;

enum class CombinationType {
    LastBeforeClose = 0,
    Saved = 1,
};

QString combinationTr(char const *sourceText, int n = -1)
{
    return QCoreApplication::translate("ShijimaManager", sourceText,
        nullptr, n);
}

QString formatSavedAt(QString const& isoDate)
{
    QDateTime savedAt = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!savedAt.isValid()) {
        return combinationTr("Not saved yet");
    }
    return QLocale::system().toString(savedAt.toLocalTime(),
        QLocale::ShortFormat);
}

QString sanitizedForLog(QString value)
{
    value = value.simplified();
    if (value.size() > 180) {
        value = value.left(177) + QStringLiteral("...");
    }
    return value;
}

QJsonObject parseCombination(QString const& payload, char const *context)
{
    if (payload.trimmed().isEmpty()) {
        return {};
    }
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        APP_LOG_WARN("combination") << "Invalid combination payload context="
            << (context == nullptr ? "unknown" : context)
            << " parseError=" << error.errorString().toStdString()
            << " size=" << payload.size()
            << " preview=" << sanitizedForLog(payload).toStdString();
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
    if (payload.trimmed().isEmpty()) {
        return {};
    }
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        APP_LOG_WARN("combination") << "Invalid saved combination list"
            << " parseError=" << error.errorString().toStdString()
            << " size=" << payload.size()
            << " preview=" << sanitizedForLog(payload).toStdString();
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

bool writeSavedCombinationArray(QSettings& settings, QJsonArray const& array)
{
    settings.setValue(kSavedCombinationsKey,
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        APP_LOG_ERROR("combination") << "Failed to persist saved combinations"
            << " status=" << static_cast<int>(settings.status())
            << " count=" << array.size();
        return false;
    }
    APP_LOG_INFO("combination") << "Saved combination list persisted count="
        << array.size();
    return true;
}

int totalMascotCount(QJsonObject const& combination)
{
    int total = 0;
    for (auto const& value : combination.value(QStringLiteral("mascots")).toArray()) {
        int count = value.toObject().value(QStringLiteral("count")).toInt();
        if (count > 0) {
            total += count;
        }
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

class ResponsiveCombinationActionRow final : public QWidget {
public:
    explicit ResponsiveCombinationActionRow(QWidget *parent = nullptr,
        int compactWidth = kCompactCombinationActionWidth):
        QWidget(parent),
        m_compactWidth(compactWidth),
        m_layout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    void addButton(ElaPushButton *button)
    {
        m_layout->addWidget(button);
    }

    void addStretch()
    {
        m_layout->addStretch();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        bool compact = width() > 0 && width() < m_compactWidth;
        m_layout->setDirection(compact
            ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }

private:
    int m_compactWidth;
    QBoxLayout *m_layout;

    Q_DISABLE_COPY_MOVE(ResponsiveCombinationActionRow)
};

class ResponsiveCombinationContent final : public QWidget {
public:
    ResponsiveCombinationContent(QWidget *list, QWidget *details,
        QWidget *parent = nullptr):
        QWidget(parent),
        m_details(details),
        m_layout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(10);
        m_layout->addWidget(list, 2);
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
        bool compact = width() > 0 && width() < kCompactCombinationWidth;
        m_layout->setDirection(compact
            ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_details->setMinimumWidth(compact ? 0 : 260);
        m_details->setMaximumWidth(compact ? QWIDGETSIZE_MAX : 340);
    }

    QWidget *m_details;
    QBoxLayout *m_layout;

    Q_DISABLE_COPY_MOVE(ResponsiveCombinationContent)
};

void configureCombinationButton(ElaPushButton *button)
{
    constexpr int horizontalPadding = 32;
    button->setMinimumHeight(38);
    button->setMinimumWidth(button->fontMetrics().horizontalAdvance(button->text())
        + button->iconSize().width() + horizontalPadding);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAccessibleName(button->text());
}

QFrame *makeCombinationPanel(QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("CombinationPanel"));
    return panel;
}

void applyCombinationTheme(QWidget *page)
{
    auto mode = eTheme->getThemeMode();
    QColor surface = ElaThemeColor(mode, BasicBase);
    QColor field = ElaThemeColor(mode, DialogLayoutArea);
    QColor disabledSurface = ElaThemeColor(mode, BasicDisable);
    QColor border = ElaThemeColor(mode, BasicBorder);
    QColor text = ElaThemeColor(mode, BasicText);
    QColor muted = ElaThemeColor(mode, BasicDetailsText);
    QColor disabledText = ElaThemeColor(mode, BasicTextDisable);
    QColor selected = ElaThemeColor(mode, PrimaryNormal);
    QColor selectedText = ElaThemeColor(mode, BasicTextInvert);

    page->setStyleSheet(QString(
        "#combinationsPage { color: %5; background: transparent; }"
        "#combinationsPage QLabel { color: %5; background: transparent; }"
        "#combinationsPage QLabel[muted=\"true\"] { color: %6; }"
        "#CombinationPanel {"
        "  background-color: %1; color: %5;"
        "  border: 1px solid %4; border-radius: 10px;"
        "}"
        "#CombinationPanel QLabel { color: %5; background: transparent; }"
        "#CombinationPanel QLabel[muted=\"true\"] { color: %6; }"
        "#CombinationPanel QListWidget {"
        "  background-color: %2; color: %5;"
        "  border: 1px solid %4; border-radius: 7px;"
        "  selection-background-color: %7; selection-color: %8;"
        "}"
        "#CombinationPanel QListWidget::item { color: %5; padding: 8px 10px; }"
        "#CombinationPanel QListWidget::item:selected {"
        "  background-color: %7; color: %8;"
        "}"
        "#CombinationPanel QListWidget:focus { border: 2px solid %7; }"
        "#CombinationPanel QListWidget:disabled {"
        "  background-color: %3; color: %9;"
        "}"
        "#CombinationPanel QScrollBar:vertical {"
        "  background: %3; width: 10px; margin: 4px 2px;"
        "}"
        "#CombinationPanel QScrollBar::handle:vertical {"
        "  background: %4; min-height: 42px; border-radius: 5px;"
        "}"
        "#CombinationPanel QScrollBar::handle:vertical:hover { background: %7; }"
        "#CombinationPanel QScrollBar::add-line:vertical, "
        "#CombinationPanel QScrollBar::sub-line:vertical { height: 0px; }"
        "#CombinationPanel QScrollBar::add-page:vertical, "
        "#CombinationPanel QScrollBar::sub-page:vertical { background: transparent; }"
        "#CombinationPanel QListWidget::item:disabled {"
        "  color: %9;"
        "}"
        "#CombinationPanel QScrollArea, #CombinationPanel QScrollArea > QWidget > QWidget {"
        "  background: transparent; border: none;"
        "}"
    ).arg(surface.name(QColor::HexArgb), field.name(QColor::HexArgb),
        disabledSurface.name(QColor::HexArgb), border.name(QColor::HexArgb),
        text.name(QColor::HexArgb), muted.name(QColor::HexArgb),
        selected.name(QColor::HexArgb), selectedText.name(QColor::HexArgb),
        disabledText.name(QColor::HexArgb)));

    auto applyPalette = [field, disabledSurface, text, muted, disabledText,
        selected, selectedText](QWidget *widget) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Base, field);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Highlight, selected);
        palette.setColor(QPalette::HighlightedText, selectedText);
        palette.setColor(QPalette::Disabled, QPalette::Base, disabledSurface);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabledText);
        widget->setPalette(palette);
    };
    for (auto *widget : page->findChildren<QLineEdit *>()) {
        applyPalette(widget);
    }
    for (auto *widget : page->findChildren<QPlainTextEdit *>()) {
        applyPalette(widget);
    }
    for (auto *widget : page->findChildren<QListWidget *>()) {
        applyPalette(widget);
    }
}

}

void ShijimaManager::setupCombinationsPage()
{
    m_ui->combinationsPage = new QWidget(this);
    m_ui->combinationsPage->setObjectName(QStringLiteral("combinationsPage"));
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
    auto *actionLayout = new QVBoxLayout(actionPanel);
    actionLayout->setContentsMargins(14, 10, 14, 10);
    actionLayout->setSpacing(8);
    auto *actionRow = new ResponsiveCombinationActionRow(actionPanel);

    auto *saveButton = new ElaPushButton(tr("Save Current Combination"), actionPanel);
    saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    configureCombinationButton(saveButton);
    connect(saveButton, &ElaPushButton::clicked, this, &ShijimaManager::saveCurrentCombination);
    actionRow->addButton(saveButton);

    auto *refreshButton = new ElaPushButton(tr("Refresh"), actionPanel);
    refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    configureCombinationButton(refreshButton);
    connect(refreshButton, &ElaPushButton::clicked, this, &ShijimaManager::refreshCombinationPage);
    actionRow->addButton(refreshButton);
    actionRow->addStretch();

    actionLayout->addWidget(actionRow);
    root->addWidget(actionPanel);

    auto *listPanel = makeCombinationPanel(m_ui->combinationsPage);
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(14, 12, 14, 12);
    listLayout->setSpacing(8);

    auto *listTitle = new QLabel(tr("Saved Combinations"), listPanel);
    listTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    listLayout->addWidget(listTitle);

    m_ui->combinationListWidget = new QListWidget(listPanel);
    m_ui->combinationListWidget->setObjectName(QStringLiteral("combinationList"));
    m_ui->combinationListWidget->setAccessibleName(tr("Saved Combinations"));
    m_ui->combinationListWidget->setSelectionMode(QListWidget::SingleSelection);
    m_ui->combinationListWidget->setUniformItemSizes(false);
    listLayout->addWidget(m_ui->combinationListWidget, 1);

    auto *detailsPanel = makeCombinationPanel(m_ui->combinationsPage);
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(14, 12, 14, 12);
    detailsLayout->setSpacing(8);

    auto *detailsTitle = new QLabel(tr("Details"), detailsPanel);
    detailsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    detailsLayout->addWidget(detailsTitle);

    m_ui->combinationDetailsLabel = new QLabel(tr("Select a combination."), detailsPanel);
    m_ui->combinationDetailsLabel->setObjectName(QStringLiteral("combinationDetails"));
    m_ui->combinationDetailsLabel->setAccessibleName(tr("Combination details"));
    m_ui->combinationDetailsLabel->setWordWrap(true);
    m_ui->combinationDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ui->combinationDetailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_ui->combinationDetailsLabel->setProperty("muted", true);
    detailsLayout->addWidget(m_ui->combinationDetailsLabel, 1);

    m_ui->restoreCombinationButton = new ElaPushButton(tr("Restore Combination"), detailsPanel);
    m_ui->restoreCombinationButton->setAccessibleName(
        m_ui->restoreCombinationButton->text());
    m_ui->restoreCombinationButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    configureCombinationButton(static_cast<ElaPushButton *>(m_ui->restoreCombinationButton));
    connect(m_ui->restoreCombinationButton, &QPushButton::clicked,
        this, &ShijimaManager::restoreSelectedCombination);
    detailsLayout->addWidget(m_ui->restoreCombinationButton);

    m_ui->deleteCombinationButton = new ElaPushButton(tr("Delete Saved Combination"), detailsPanel);
    m_ui->deleteCombinationButton->setAccessibleName(
        m_ui->deleteCombinationButton->text());
    m_ui->deleteCombinationButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    configureCombinationButton(static_cast<ElaPushButton *>(m_ui->deleteCombinationButton));
    connect(m_ui->deleteCombinationButton, &QPushButton::clicked,
        this, &ShijimaManager::deleteSelectedCombination);
    detailsLayout->addWidget(m_ui->deleteCombinationButton);

    auto *responsiveContent = new ResponsiveCombinationContent(
        listPanel, detailsPanel, m_ui->combinationsPage);
    root->addWidget(responsiveContent, 1);

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
                    item->data(kCombinationPayloadRole).toString(), "selection-details");
                m_ui->combinationDetailsLabel->setText(
                    combinationDetails(item->text().section(QLatin1Char('\n'), 0, 0),
                        combination));
            }
        }
    });

    refreshCombinationPage();
    applyCombinationTheme(m_ui->combinationsPage);
    connect(eTheme, &ElaTheme::themeModeChanged, m_ui->combinationsPage, [this]() {
        applyCombinationTheme(m_ui->combinationsPage);
    });
    addPageNode(tr("Combinations"), m_ui->combinationsPage, ElaIconType::ObjectGroup);
}

void ShijimaManager::refreshCombinationPage()
{
    if (m_ui->combinationListWidget == nullptr) {
        APP_LOG_WARN("combination") << "Refresh requested before combination list was initialized";
        return;
    }

    m_ui->combinationListWidget->clear();

    QJsonObject lastCombination = parseCombination(
        m_settings->value(kLastCombinationKey).toString(), "last-before-close");
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
    int skipped = 0;
    for (auto const& value : saved) {
        QJsonObject entry = value.toObject();
        QJsonObject combination = entry.value(QStringLiteral("combination")).toObject();
        if (combination.isEmpty() || !combination.value(QStringLiteral("mascots")).isArray()) {
            ++skipped;
            APP_LOG_WARN("combination") << "Skipping invalid saved combination entry id="
                << entry.value(QStringLiteral("id")).toString().toStdString();
            continue;
        }
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
    APP_LOG_DEBUG("combination") << "Combination page refreshed saved="
        << saved.size() << " skipped=" << skipped
        << " lastRestorable=" << (totalMascotCount(lastCombination) > 0);

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
    if (m_settings == nullptr || m_runtime == nullptr) {
        APP_LOG_ERROR("combination") << "Cannot save combination; manager state is incomplete";
        return;
    }

    QJsonObject combination;
    try {
        combination = currentCombinationObject(*m_runtime);
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("combination") << "Failed to collect current combination: "
            << ex.what();
        ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
            tr("Could not save the current combination."));
        return;
    }
    catch (...) {
        APP_LOG_ERROR("combination") << "Failed to collect current combination: unknown exception";
        ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
            tr("Could not save the current combination."));
        return;
    }

    int mascotCount = totalMascotCount(combination);
    APP_LOG_INFO("combination") << "Save current combination requested mascotCount="
        << mascotCount;
    if (totalMascotCount(combination) == 0) {
        ShijimaManagerUiInternal::showThemedInformation(this,
            tr("Combinations"),
            tr("There are no active mascots to save."));
        return;
    }

    QString defaultName = tr("Combination %1")
        .arg(QLocale::system().toString(QDateTime::currentDateTime(),
            QLocale::ShortFormat));
    QString name;
    if (!ShijimaManagerUiInternal::showThemedTextInput(this,
            tr("Save Combination"), tr("Combination name:"), defaultName,
            &name, tr("Save"), tr("Cancel"))) {
        return;
    }
    name = name.trimmed();
    if (name.isEmpty()) {
        name = defaultName;
    }

    QJsonArray saved = savedCombinationArray(*m_settings);
    QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    saved.append(QJsonObject {
        { QStringLiteral("id"), id },
        { QStringLiteral("name"), name },
        { QStringLiteral("combination"), combination },
    });
    if (!writeSavedCombinationArray(*m_settings, saved)) {
        ShijimaManagerUiInternal::showThemedWarning(this,
            tr("Combinations"),
            tr("Could not save the combination settings."));
        return;
    }
    APP_LOG_INFO("combination") << "Saved current combination id="
        << id.toStdString() << " name=" << sanitizedForLog(name).toStdString()
        << " mascotCount=" << mascotCount;
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

    QJsonObject combination = parseCombination(
        item->data(kCombinationPayloadRole).toString(), "selected-combination");
    if (totalMascotCount(combination) == 0) {
        ShijimaManagerUiInternal::showThemedInformation(this,
            tr("Combinations"),
            tr("This combination does not contain any mascots."));
        return;
    }

    try {
        restoreCombination(combination, true);
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("combination") << "Failed to restore selected combination: "
            << ex.what();
        ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
            tr("Could not restore this combination."));
    }
    catch (...) {
        APP_LOG_ERROR("combination") << "Failed to restore selected combination: unknown exception";
        ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
            tr("Could not restore this combination."));
    }
}

int ShijimaManager::restoreCombination(QJsonObject const& combination, bool showMessages)
{
    int requestedTotal = totalMascotCount(combination);
    if (requestedTotal == 0) {
        APP_LOG_DEBUG("combination") << "Restore skipped for empty combination";
        return 0;
    }

    APP_LOG_INFO("combination") << "Restoring combination requestedTotal="
        << requestedTotal << " showMessages=" << showMessages;

    try {
        killAll();
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("combination") << "Failed to clear running mascots before restore: "
            << ex.what();
        if (showMessages) {
            ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
                tr("Could not clear the current mascots before restoring."));
        }
        return 0;
    }
    catch (...) {
        APP_LOG_ERROR("combination") << "Failed to clear running mascots before restore: unknown exception";
        if (showMessages) {
            ShijimaManagerUiInternal::showThemedWarning(this, tr("Combinations"),
                tr("Could not clear the current mascots before restoring."));
        }
        return 0;
    }

    QStringList missing;
    QStringList failed;
    int restored = 0;
    int attempted = 0;
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
        if (count > kMaxMascotsPerEntry) {
            APP_LOG_WARN("combination") << "Clamping excessive mascot count name="
                << name.toStdString() << " requested=" << count
                << " limit=" << kMaxMascotsPerEntry;
            count = kMaxMascotsPerEntry;
        }
        for (int i = 0; i < count; ++i) {
            if (attempted >= kMaxMascotsPerCombination) {
                APP_LOG_WARN("combination") << "Combination restore reached safety limit limit="
                    << kMaxMascotsPerCombination;
                break;
            }
            ++attempted;
            try {
                if (spawn(name.toStdString()) != nullptr) {
                    ++restored;
                }
                else {
                    failed.append(name);
                }
            }
            catch (std::exception const& ex) {
                failed.append(name);
                APP_LOG_ERROR("combination") << "Mascot spawn failed during combination restore name="
                    << name.toStdString() << " error=" << ex.what();
            }
            catch (...) {
                failed.append(name);
                APP_LOG_ERROR("combination") << "Mascot spawn failed during combination restore name="
                    << name.toStdString() << " error=unknown";
            }
        }
        if (attempted >= kMaxMascotsPerCombination) {
            break;
        }
    }

    updateStatusBar();
    missing.removeDuplicates();
    failed.removeDuplicates();
    APP_LOG_INFO("combination") << "Combination restore finished requested="
        << requestedTotal << " attempted=" << attempted << " restored=" << restored
        << " missing=" << missing.size() << " failed=" << failed.size();
    if (showMessages && !missing.isEmpty()) {
        ShijimaManagerUiInternal::showThemedWarning(this,
            tr("Combinations"),
            combinationTr("Restored %n mascot(s). Missing templates: %1", restored)
                .arg(missing.join(QStringLiteral(", "))));
    }
    else if (showMessages && !failed.isEmpty()) {
        ShijimaManagerUiInternal::showThemedWarning(this,
            tr("Combinations"),
            combinationTr("Restored %n mascot(s). Some mascots could not be started: %1",
                restored)
                .arg(failed.join(QStringLiteral(", "))));
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
        combination = parseCombination(m_settings->value(kLastCombinationKey).toString(),
            "startup-last");
    }
    else if (mode == QStringLiteral("saved")) {
        QString id = m_settings->value(QStringLiteral("startup/restoreCombinationId")).toString();
        combination = savedCombinationById(*m_settings, id);
        if (combination.isEmpty()) {
            APP_LOG_WARN("combination") << "Startup saved combination not found id="
                << id.toStdString();
        }
    }
    else {
        APP_LOG_WARN("combination") << "Unknown startup combination restore mode="
            << mode.toStdString();
        return;
    }

    try {
        int restored = restoreCombination(combination, false);
        APP_LOG_INFO("combination") << "Startup combination restore finished mode="
            << mode.toStdString() << " restored=" << restored;
    }
    catch (std::exception const& ex) {
        APP_LOG_ERROR("combination") << "Startup combination restore failed mode="
            << mode.toStdString() << " error=" << ex.what();
    }
    catch (...) {
        APP_LOG_ERROR("combination") << "Startup combination restore failed mode="
            << mode.toStdString() << " error=unknown";
    }
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

    if (!ShijimaManagerUiInternal::showThemedQuestion(this,
        tr("Delete Combination"),
        tr("Delete this saved combination?"), tr("Delete"), tr("Cancel"),
        true))
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
    if (!writeSavedCombinationArray(*m_settings, updated)) {
        ShijimaManagerUiInternal::showThemedWarning(this,
            tr("Delete Combination"),
            tr("Could not update the saved combinations."));
        return;
    }
    APP_LOG_INFO("combination") << "Deleted saved combination id="
        << id.toStdString() << " remaining=" << updated.size();
    refreshCombinationPage();
}

void ShijimaManager::saveLastCombinationBeforeShutdown()
{
    if (m_settings == nullptr || m_runtime == nullptr) {
        return;
    }
    QJsonObject combination = currentCombinationObject(*m_runtime);
    m_settings->setValue(kLastCombinationKey, combinationToPayload(combination));
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        APP_LOG_ERROR("combination") << "Failed to persist last combination before shutdown"
            << " status=" << static_cast<int>(m_settings->status());
        return;
    }
    APP_LOG_INFO("combination") << "Last combination persisted before shutdown mascotCount="
        << totalMascotCount(combination);
}
