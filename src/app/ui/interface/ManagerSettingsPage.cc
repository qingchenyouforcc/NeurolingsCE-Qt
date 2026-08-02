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
#include "shijima-qt/CodexActivity.hpp"
#include "shijima-qt/CodexConfigManager.hpp"
#include "../../core/update/GitHubUpdateManager.hpp"
#include "../../runtime/ManagerRuntimeState.hpp"
#include "../ManagerUiState.hpp"
#include "../ManagerUiHelpers.hpp"
#include <QAction>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFrame>
#include <QFormLayout>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOptionFocusRect>
#include <QVariant>
#include <QVBoxLayout>
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"

namespace {

void drawKeyboardFocusFrame(QWidget *widget)
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

class SettingsPushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawKeyboardFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(SettingsPushButton)
};

class SettingsToggleSwitch final : public ElaToggleSwitch {
public:
    explicit SettingsToggleSwitch(QWidget *parent = nullptr):
        ElaToggleSwitch(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (!event->isAutoRepeat() &&
            (event->key() == Qt::Key_Space ||
                event->key() == Qt::Key_Return ||
                event->key() == Qt::Key_Enter))
        {
            setIsToggled(!getIsToggled());
            event->accept();
            return;
        }
        ElaToggleSwitch::keyPressEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        ElaToggleSwitch::paintEvent(event);
        drawKeyboardFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(SettingsToggleSwitch)
};

QString settingsTr(char const* sourceText)
{
    return QCoreApplication::translate("ShijimaManager", sourceText);
}

struct SettingsColors {
    QString panelBg;
    QString panelBorder;
    QString details;
    QString scrollTrack;
    QString scrollThumb;
    QString scrollThumbHover;
};

SettingsColors themedSettingsColors()
{
    auto mode = eTheme->getThemeMode();
    return SettingsColors {
        ElaThemeColor(mode, WindowBase).name(),
        ElaThemeColor(mode, BasicBorder).name(),
        ElaThemeColor(mode, BasicDetailsText).name(),
        ElaThemeColor(mode, BasicBase).name(),
        ElaThemeColor(mode, BasicBorder).name(),
        ElaThemeColor(mode, PrimaryNormal).name(),
    };
}

QString formatNumber(double value, int decimals)
{
    QString text = QString::number(value, 'f', decimals);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

ElaText *makeSectionTitle(QWidget *parent, QString const& text)
{
    auto *title = new ElaText(text, parent);
    title->setTextPixelSize(20);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("#ElaText { background-color: transparent; border: none; }"));
    return title;
}

QLabel *makeSectionDescription(QWidget *parent, QString const& text)
{
    auto colors = themedSettingsColors();
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral(
        "background-color: transparent; border: none; color: %1;").arg(colors.details));
    return label;
}

QWidget *createSettingsRow(QWidget *parent, QString const& title,
    QString const& subtitle, QWidget *control, QLabel **subtitleLabelOut = nullptr)
{
    control->setAccessibleName(title);
    control->setAccessibleDescription(subtitle);

    auto colors = themedSettingsColors();
    auto *area = new QFrame(parent);
    area->setObjectName(QStringLiteral("SettingsRowCard"));
    area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    area->setStyleSheet(QString(
        "#SettingsRowCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 14px;"
        "}"
    ).arg(colors.panelBg, colors.panelBorder));

    auto *row = new QHBoxLayout(area);
    row->setContentsMargins(18, 14, 18, 14);
    row->setSpacing(16);

    auto *textColumn = new QVBoxLayout;
    textColumn->setSpacing(4);

    auto *titleLabel = new ElaText(title, area);
    titleLabel->setTextPixelSize(15);
    titleLabel->setWordWrap(false);
    titleLabel->setStyleSheet(QStringLiteral("#ElaText { background-color: transparent; border: none; }"));

    auto *subtitleLabel = new QLabel(subtitle, area);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral(
        "background-color: transparent; border: none; color: %1;").arg(colors.details));

    textColumn->addWidget(titleLabel);
    textColumn->addWidget(subtitleLabel);
    row->addLayout(textColumn, 1);
    row->addWidget(control, 0, Qt::AlignVCenter);

    if (subtitleLabelOut != nullptr) {
        *subtitleLabelOut = subtitleLabel;
    }
    return area;
}

void updateSettingsSummary(QLabel *label, QWidget *control, QString const& summary)
{
    if (label != nullptr) {
        label->setText(summary);
    }
    if (control != nullptr) {
        control->setAccessibleDescription(summary);
    }
}

void addSettingsSection(QVBoxLayout *layout, QWidget *parent, QString const& title,
    QString const& description)
{
    layout->addSpacing(6);
    layout->addWidget(makeSectionTitle(parent, title));
    layout->addWidget(makeSectionDescription(parent, description));
}

QString proxySummaryForSettings(QSettings const& settings)
{
    QString mode = settings.value(QStringLiteral("update/proxyMode"),
        QStringLiteral("system")).toString().trimmed().toLower();
    if (mode == QStringLiteral("direct")) {
        return settingsTr("Connect directly without a proxy.");
    }
    if (mode == QStringLiteral("system") || mode.isEmpty()) {
        return settingsTr("Use the operating system proxy configuration.");
    }

    QString host = settings.value(QStringLiteral("update/proxyHost")).toString().trimmed();
    int port = settings.value(QStringLiteral("update/proxyPort"), 8080).toInt();
    if (host.isEmpty() || port <= 0) {
        return settingsTr("Manual proxy is enabled, but the host or port is incomplete.");
    }

    if (mode == QStringLiteral("socks5")) {
        return settingsTr("SOCKS5 proxy %1:%2").arg(host).arg(port);
    }
    return settingsTr("HTTP proxy %1:%2").arg(host).arg(port);
}

QString languageSummaryForSettings(QString const& language)
{
    return language == QStringLiteral("zh_CN")
        ? settingsTr("Simplified Chinese")
        : settingsTr("English");
}

QString scaleSummaryForSettings(double scale)
{
    return settingsTr("Current scale: %1x").arg(formatNumber(scale, 3));
}

QString detachSummaryForSettings(int threshold)
{
    return settingsTr("Current threshold: %1 px/tick").arg(threshold);
}

QString backgroundSummaryForSettings(QColor const& color)
{
    return settingsTr("Current color: %1")
        .arg(ShijimaManagerUiInternal::colorToString(color));
}

QString quoteProcessArgument(QString const& argument)
{
    QString escaped = argument;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString startupRunCommand()
{
    return quoteProcessArgument(QCoreApplication::applicationFilePath()) +
        QStringLiteral(" --neurolingsce-startup");
}

QString codexCliExecutablePath()
{
    QString name = QStringLiteral("NeurolingsCE-cli");
#ifdef _WIN32
    name += QStringLiteral(".exe");
#endif
    return QFileInfo(QCoreApplication::applicationDirPath() +
        QDir::separator() + name).absoluteFilePath();
}

QString codexTemplateLabel(QString const& name)
{
    return name == QStringLiteral("@")
        ? settingsTr("Default Mascot") : name;
}

QString codexDefaultTemplateName()
{
    return QStringLiteral("@");
}

bool startupLaunchAtLoginEnabled()
{
#ifdef _WIN32
    QSettings runKey(QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    return !runKey.value(QStringLiteral("NeurolingsCE")).toString().trimmed().isEmpty();
#else
    return false;
#endif
}

bool setStartupLaunchAtLoginEnabled(bool enabled, QString &errorMessage)
{
#ifdef _WIN32
    QSettings runKey(QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue(QStringLiteral("NeurolingsCE"), startupRunCommand());
    }
    else {
        runKey.remove(QStringLiteral("NeurolingsCE"));
    }
    runKey.sync();
    if (runKey.status() != QSettings::NoError) {
        errorMessage = settingsTr("Could not update the system startup setting.");
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled);
    errorMessage = settingsTr("Startup launch is currently supported on Windows only.");
    return false;
#endif
}

QString startupLaunchSummary()
{
    return startupLaunchAtLoginEnabled()
        ? settingsTr("Enabled")
        : settingsTr("Disabled");
}

QJsonArray savedCombinationArrayForSettings(QSettings const& settings)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(
        settings.value(QStringLiteral("combinations/saved")).toString().toUtf8(),
        &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }
    return document.array();
}

QString savedCombinationName(QSettings const& settings, QString const& id)
{
    for (auto const& value : savedCombinationArrayForSettings(settings)) {
        QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() == id) {
            QString name = entry.value(QStringLiteral("name")).toString().trimmed();
            return name.isEmpty() ? settingsTr("Untitled Combination") : name;
        }
    }
    return {};
}

QString startupCombinationSummary(QSettings const& settings)
{
    QString mode = settings.value(QStringLiteral("startup/restoreCombinationMode"),
        QStringLiteral("last")).toString();
    if (mode == QStringLiteral("none")) {
        return settingsTr("Do not restore a combination.");
    }
    if (mode == QStringLiteral("saved")) {
        QString name = savedCombinationName(settings,
            settings.value(QStringLiteral("startup/restoreCombinationId")).toString());
        if (!name.isEmpty()) {
            return settingsTr("Restore saved combination: %1").arg(name);
        }
        return settingsTr("Restore a saved combination, but the selection is missing.");
    }
    return settingsTr("Restore the last combination before close.");
}

void populateStartupCombinationCombo(QComboBox *combo, QSettings const& settings)
{
    combo->clear();
    combo->addItem(settingsTr("Do not restore"), QStringLiteral("none:"));
    combo->addItem(settingsTr("Last Combination Before Close"), QStringLiteral("last:"));
    for (auto const& value : savedCombinationArrayForSettings(settings)) {
        QJsonObject entry = value.toObject();
        QString id = entry.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        QString name = entry.value(QStringLiteral("name")).toString().trimmed();
        combo->addItem(name.isEmpty() ? settingsTr("Untitled Combination") : name,
            QStringLiteral("saved:%1").arg(id));
    }

    QString mode = settings.value(QStringLiteral("startup/restoreCombinationMode"),
        QStringLiteral("last")).toString();
    QString id = settings.value(QStringLiteral("startup/restoreCombinationId")).toString();
    QString target = mode == QStringLiteral("saved")
        ? QStringLiteral("saved:%1").arg(id)
        : QStringLiteral("%1:").arg(mode);
    int index = combo->findData(target);
    combo->setCurrentIndex(index >= 0 ? index : 1);
}

}

void ShijimaManager::setupSettingsPage() {
    auto *settingsScrollArea = new QScrollArea(this);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setFrameShape(QFrame::NoFrame);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto colors = themedSettingsColors();
    settingsScrollArea->setStyleSheet(QString(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 10px;"
        "  margin: 4px 2px 4px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %1;"
        "  min-height: 48px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %2;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: %3;"
        "  border-radius: 5px;"
        "}"
    ).arg(colors.scrollThumb, colors.scrollThumbHover, colors.scrollTrack));

    auto *settingsContent = new QWidget(settingsScrollArea);
    settingsContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *settingsLayout = new QVBoxLayout(settingsContent);
    settingsLayout->setContentsMargins(16, 14, 16, 14);
    settingsLayout->setSpacing(10);
    settingsLayout->setAlignment(Qt::AlignTop);
    settingsScrollArea->setWidget(settingsContent);
    m_ui->settingsPage = settingsScrollArea;

    addSettingsSection(settingsLayout, settingsContent,
        tr("Interaction"),
        tr("Tune how mascots multiply, speak, and respond to your clicks."));

    {
        static const QString key = "multiplicationEnabled";
        bool initial = m_settings->value(key, QVariant::fromValue(true)).toBool();
        m_runtime->environment.setAllowsBreeding(initial);

        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(initial);
        connect(toggle, &ElaToggleSwitch::toggled, [this](bool checked) {
            m_runtime->environment.setAllowsBreeding(checked);
            m_settings->setValue("multiplicationEnabled", QVariant::fromValue(checked));
        });

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Multiplication"),
            tr("Allow mascots to create additional companions."),
            toggle));
    }

    {
        static const QString key = "speechBubbleEnabled";
        bool initial = m_settings->value(key, QVariant::fromValue(true)).toBool();

        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(initial);
        connect(toggle, &ElaToggleSwitch::toggled, [this](bool checked) {
            m_settings->setValue("speechBubbleEnabled", QVariant::fromValue(checked));
        });

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Speech Bubble"),
            tr("Show mascot dialogue bubbles when interactions trigger them."),
            toggle));
    }

    {
        static const QString key = "speechBubbleClickCount";
        int initial = m_settings->value(key, 1).toInt();

        auto *spinBox = new QSpinBox(settingsContent);
        spinBox->setRange(1, 10);
        spinBox->setValue(initial);
        spinBox->setMinimumWidth(88);
        connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
            m_settings->setValue("speechBubbleClickCount", val);
        });

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Speech Bubble Click Count"),
            tr("Choose how many clicks are needed before a bubble appears."),
            spinBox));
    }

    addSettingsSection(settingsLayout, settingsContent,
        tr("Codex"),
        tr("Show Codex completion messages through a dedicated mascot bubble. "
            "Codex approval requests are not handled by this integration."));

    {
        bool initial = m_settings->value(QStringLiteral("codex/enabled"), false).toBool();
        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(initial);
        connect(toggle, &ElaToggleSwitch::toggled, this,
            [this, toggle](bool checked) {
                QString configPath = codexConfigPath();
                QString executable = codexCliExecutablePath();
                if (checked) {
                    QString prompt = tr(
                        "Allow NeurolingsCE to update the Codex user configuration?\n\n"
                        "Path: %1\nCommand: %2")
                        .arg(configPath, codexNotifyCommand(executable));
                    if (QMessageBox::question(this, tr("Enable Codex notifications"),
                        prompt, QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes) != QMessageBox::Yes)
                    {
                        toggle->blockSignals(true);
                        toggle->setIsToggled(false);
                        toggle->blockSignals(false);
                        return;
                    }
                    auto result = enableCodexNotify(configPath, executable);
                    if (!result.ok) {
                        toggle->blockSignals(true);
                        toggle->setIsToggled(false);
                        toggle->blockSignals(false);
                        QString message = result.error;
                        if (result.conflict) {
                            message += tr("\n\nCopy this line into the configuration manually if desired:\n%1")
                                .arg(result.snippet);
                        }
                        QMessageBox::warning(this, tr("Codex notifications"), message);
                        return;
                    }
                    m_settings->setValue(QStringLiteral("codex/enabled"), true);
                    return;
                }

                auto result = disableCodexNotify(configPath);
                if (!result.ok) {
                    toggle->blockSignals(true);
                    toggle->setIsToggled(true);
                    toggle->blockSignals(false);
                    QMessageBox::warning(this, tr("Codex notifications"), result.error);
                    return;
                }
                m_settings->setValue(QStringLiteral("codex/enabled"), false);
            });
        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Enable Codex message bubbles"),
            tr("Install or remove only NeurolingsCE's managed notify block after confirmation."),
            toggle));
    }

    {
        auto *combo = new QComboBox(settingsContent);
        QString configured = m_settings->value(
            QStringLiteral("codex/companionTemplate"), codexDefaultTemplateName())
            .toString();
        for (auto const& name : m_runtime->templates.loadedMascots().keys()) {
            combo->addItem(codexTemplateLabel(name), name);
        }
        if (combo->findData(configured) < 0 && !configured.isEmpty()) {
            combo->addItem(tr("Missing: %1 (will use Default Mascot)").arg(configured),
                configured);
        }
        int configuredIndex = combo->findData(configured);
        if (configuredIndex < 0 && combo->count() > 0) {
            configuredIndex = combo->findData(codexDefaultTemplateName());
        }
        if (configuredIndex >= 0) {
            combo->setCurrentIndex(configuredIndex);
        }
        combo->setMinimumWidth(180);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, combo](int index) {
                if (index >= 0) {
                    m_settings->setValue(QStringLiteral("codex/companionTemplate"),
                        combo->itemData(index).toString());
                }
            });
        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Codex companion template"),
            tr("Reuse the earliest running mascot of this template, or summon one when needed."),
            combo));
    }

    {
        auto *button = new SettingsPushButton(tr("Send test notification"), settingsContent);
        int textWidth = button->fontMetrics().horizontalAdvance(button->text());
        button->setMinimumWidth(qMax(160, textWidth + 40));
        button->setMinimumHeight(qMax(36, button->sizeHint().height()));
        connect(button, &ElaPushButton::clicked, this, [this]() {
            if (!m_settings->value(QStringLiteral("codex/enabled"), false).toBool()) {
                QMessageBox::information(this, tr("Codex notifications"),
                    tr("Enable Codex message bubbles first."));
                return;
            }
            CodexActivity activity;
            activity.type = QStringLiteral("agent-turn-complete");
            activity.state = CodexActivityState::Ready;
            activity.lastAssistantMessage = tr("This is a Codex test notification.");
            if (!showCodexNotification(activity)) {
                QMessageBox::warning(this, tr("Codex notifications"),
                    tr("No mascot was available to display the test notification."));
            }
        });
        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Test Codex message"),
            tr("Preview the title, excerpt, and eight-second queue behavior."),
            button));
    }

    {
        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Edit..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Detach Speed"),
            detachSummaryForSettings(static_cast<int>(m_runtime->environment.detachThreshold())),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            static const QString key = "detachThreshold";
            int threshold = static_cast<int>(m_runtime->environment.detachThreshold());
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Detach Speed"));
            dialog.setMinimumWidth(360);

            auto *layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel(tr("Threshold (px/tick):"), &dialog));

            auto *slider = new QSlider(Qt::Horizontal, &dialog);
            slider->setRange(0, 200);
            slider->setValue(threshold);

            auto *spin = new QSpinBox(&dialog);
            spin->setRange(0, 200);
            spin->setValue(threshold);

            connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);

            layout->addWidget(slider);
            layout->addWidget(spin);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);

            if (dialog.exec() == QDialog::Accepted) {
                m_runtime->environment.setDetachThreshold(spin->value());
                m_settings->setValue(key, m_runtime->environment.detachThreshold());
                updateSettingsSummary(summaryLabel, btn, detachSummaryForSettings(
                    static_cast<int>(m_runtime->environment.detachThreshold())));
            }
        });

        settingsLayout->addWidget(row);
    }

    addSettingsSection(settingsLayout, settingsContent,
        tr("Display"),
        tr("Adjust the manager presentation, sandbox appearance, and application language."));

    {
        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(windowedMode());

        if (!m_ui->windowedModeAction) {
            m_ui->windowedModeAction = new QAction(this);
        }
        m_ui->windowedModeAction->setCheckable(true);
        m_ui->windowedModeAction->setChecked(windowedMode());

        connect(toggle, &ElaToggleSwitch::toggled, [this](bool checked) {
            setWindowedMode(checked);
        });
        connect(m_ui->windowedModeAction, &QAction::toggled, toggle, &ElaToggleSwitch::setIsToggled);

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Windowed Mode"),
            tr("Keep mascots inside the sandbox window instead of the desktop."),
            toggle));
    }

    {
        static const QString key = "windowedModeBackground";
        QColor initial = m_settings->value(key, "#FF0000").toString();
        m_ui->sandboxBackground = initial;
        updateSandboxBackground();

        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Edit..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Background Color"),
            backgroundSummaryForSettings(m_ui->sandboxBackground),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            QColorDialog dialog { this };
            dialog.setCurrentColor(m_ui->sandboxBackground);
            if (dialog.exec() == QDialog::Accepted) {
                m_ui->sandboxBackground = dialog.selectedColor();
                m_settings->setValue("windowedModeBackground",
                    ShijimaManagerUiInternal::colorToString(dialog.selectedColor()));
                updateSandboxBackground();
                updateSettingsSummary(summaryLabel, btn,
                    backgroundSummaryForSettings(m_ui->sandboxBackground));
            }
        });

        settingsLayout->addWidget(row);
    }

    {
        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Edit..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Scale"),
            scaleSummaryForSettings(m_runtime->environment.userScale()),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            static const QString key = "userScale";
            double scale = m_runtime->environment.userScale();
            QDialog dialog { this };
            dialog.setWindowTitle(tr("Custom Scale"));
            dialog.setMinimumWidth(360);
            auto *mainLayout = new QVBoxLayout(&dialog);

            mainLayout->addWidget(new QLabel(tr("Adjust Scale:"), &dialog));

            auto *slider = new QSlider(Qt::Horizontal, &dialog);
            slider->setRange(100, 10000);
            slider->setValue(static_cast<int>(scale * 1000));

            auto *spin = new QDoubleSpinBox(&dialog);
            spin->setRange(0.1, 10.0);
            spin->setDecimals(3);
            spin->setSingleStep(0.05);
            spin->setValue(scale);

            connect(slider, &QSlider::valueChanged, [this, spin](int v) {
                m_runtime->environment.setUserScale(v / 1000.0);
                spin->blockSignals(true);
                spin->setValue(m_runtime->environment.userScale());
                spin->blockSignals(false);
            });
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, slider](double v) {
                    m_runtime->environment.setUserScale(v);
                    slider->blockSignals(true);
                    slider->setValue(static_cast<int>(v * 1000));
                    slider->blockSignals(false);
                });

            mainLayout->addWidget(slider);
            mainLayout->addWidget(spin);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            mainLayout->addWidget(buttons);

            if (dialog.exec() == QDialog::Accepted) {
                m_settings->setValue(key, m_runtime->environment.userScale());
                updateSettingsSummary(summaryLabel, btn,
                    scaleSummaryForSettings(m_runtime->environment.userScale()));
            }
        });

        settingsLayout->addWidget(row);
    }

    {
        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Edit..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Language"),
            languageSummaryForSettings(m_ui->currentLanguage),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Select Language"));
            dialog.setMinimumWidth(320);

            auto *layout = new QVBoxLayout(&dialog);
            auto *btnEn = new QRadioButton(tr("English"), &dialog);
            auto *btnZh = new QRadioButton(tr("Simplified Chinese"), &dialog);

            if (m_ui->currentLanguage == QStringLiteral("zh_CN")) {
                btnZh->setChecked(true);
            }
            else {
                btnEn->setChecked(true);
            }

            layout->addWidget(btnEn);
            layout->addWidget(btnZh);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);

            if (dialog.exec() == QDialog::Accepted) {
                updateSettingsSummary(summaryLabel, btn, languageSummaryForSettings(
                    btnZh->isChecked() ? QStringLiteral("zh_CN") : QStringLiteral("en")));
                switchLanguage(btnZh->isChecked() ? QStringLiteral("zh_CN") : QStringLiteral("en"));
            }
        });

        settingsLayout->addWidget(row);
    }

    addSettingsSection(settingsLayout, settingsContent,
        tr("Startup"),
        tr("Control how NeurolingsCE starts with your system and what it restores."));

    {
        QLabel *summaryLabel = nullptr;
        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(startupLaunchAtLoginEnabled());
        auto *row = createSettingsRow(settingsContent,
            tr("Start at Login"),
            startupLaunchSummary(),
            toggle,
            &summaryLabel);
        connect(toggle, &ElaToggleSwitch::toggled, this,
            [this, toggle, summaryLabel](bool checked) {
                QString errorMessage;
                if (!setStartupLaunchAtLoginEnabled(checked, errorMessage)) {
                    toggle->blockSignals(true);
                    toggle->setIsToggled(!checked);
                    toggle->blockSignals(false);
                    QMessageBox::warning(this, tr("Startup"), errorMessage);
                }
                updateSettingsSummary(summaryLabel, toggle, startupLaunchSummary());
            });
        settingsLayout->addWidget(row);
    }

    {
        bool initial = m_settings->value("startup/silent", false).toBool();
        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(initial);
        connect(toggle, &ElaToggleSwitch::toggled, [this](bool checked) {
            m_settings->setValue("startup/silent", checked);
        });

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Silent Startup"),
            tr("When launched at login, keep the manager in the tray and restore the configured combination."),
            toggle));
    }

    {
        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Configure..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Startup Combination"),
            startupCombinationSummary(*m_settings),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Startup Combination"));
            dialog.setMinimumWidth(420);

            auto *layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel(
                tr("Choose which combination to restore during silent startup."),
                &dialog));

            auto *combo = new QComboBox(&dialog);
            populateStartupCombinationCombo(combo, *m_settings);
            layout->addWidget(combo);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);

            if (dialog.exec() == QDialog::Accepted) {
                QString data = combo->currentData().toString();
                QString mode = data.section(QLatin1Char(':'), 0, 0);
                QString id = data.section(QLatin1Char(':'), 1);
                m_settings->setValue("startup/restoreCombinationMode", mode);
                m_settings->setValue("startup/restoreCombinationId", id);
                updateSettingsSummary(summaryLabel, btn,
                    startupCombinationSummary(*m_settings));
            }
        });
        settingsLayout->addWidget(row);
    }

    addSettingsSection(settingsLayout, settingsContent,
        tr("Updates"),
        tr("Control how NeurolingsCE checks GitHub releases and connects to the network."));

    {
        bool initial = m_settings->value("update/checkOnStartup", true).toBool();
        auto *toggle = new SettingsToggleSwitch(settingsContent);
        toggle->setIsToggled(initial);
        connect(toggle, &ElaToggleSwitch::toggled, [this](bool checked) {
            m_settings->setValue("update/checkOnStartup", checked);
        });

        settingsLayout->addWidget(createSettingsRow(settingsContent,
            tr("Check for Updates on Startup"),
            tr("Automatically check GitHub releases when the app launches."),
            toggle));
    }

    {
        QLabel *summaryLabel = nullptr;
        auto *btn = new SettingsPushButton(tr("Configure..."), settingsContent);
        auto *row = createSettingsRow(settingsContent,
            tr("Update Proxy"),
            proxySummaryForSettings(*m_settings),
            btn,
            &summaryLabel);
        connect(btn, &ElaPushButton::clicked, [this, btn, summaryLabel]() {
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Update Proxy"));
            dialog.setMinimumWidth(420);

            auto *layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel(
                tr("This proxy is only used for GitHub update checks and downloads."),
                &dialog));

            auto *form = new QFormLayout;
            form->setLabelAlignment(Qt::AlignLeft);
            form->setFormAlignment(Qt::AlignTop);

            auto *modeCombo = new QComboBox(&dialog);
            modeCombo->addItem(tr("Use system proxy"), QStringLiteral("system"));
            modeCombo->addItem(tr("Direct connection"), QStringLiteral("direct"));
            modeCombo->addItem(tr("HTTP proxy"), QStringLiteral("http"));
            modeCombo->addItem(tr("SOCKS5 proxy"), QStringLiteral("socks5"));

            int currentIndex = modeCombo->findData(
                m_settings->value("update/proxyMode", QStringLiteral("system")).toString());
            if (currentIndex < 0) {
                currentIndex = 0;
            }
            modeCombo->setCurrentIndex(currentIndex);

            auto *hostEdit = new QLineEdit(
                m_settings->value("update/proxyHost").toString(), &dialog);
            auto *portSpin = new QSpinBox(&dialog);
            portSpin->setRange(1, 65535);
            portSpin->setValue(m_settings->value("update/proxyPort", 8080).toInt());
            auto *userEdit = new QLineEdit(
                m_settings->value("update/proxyUsername").toString(), &dialog);
            auto *passwordEdit = new QLineEdit(
                m_settings->value("update/proxyPassword").toString(), &dialog);
            passwordEdit->setEchoMode(QLineEdit::Password);

            form->addRow(tr("Mode"), modeCombo);
            form->addRow(tr("Host"), hostEdit);
            form->addRow(tr("Port"), portSpin);
            form->addRow(tr("Username"), userEdit);
            form->addRow(tr("Password"), passwordEdit);
            layout->addLayout(form);

            auto updateFieldState = [modeCombo, hostEdit, portSpin, userEdit, passwordEdit]() {
                QString mode = modeCombo->currentData().toString();
                bool manual = mode == QStringLiteral("http") || mode == QStringLiteral("socks5");
                hostEdit->setEnabled(manual);
                portSpin->setEnabled(manual);
                userEdit->setEnabled(manual);
                passwordEdit->setEnabled(manual);
            };
            updateFieldState();
            connect(modeCombo, &QComboBox::currentIndexChanged, &dialog, updateFieldState);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);

            if (dialog.exec() == QDialog::Accepted) {
                m_settings->setValue("update/proxyMode", modeCombo->currentData().toString());
                m_settings->setValue("update/proxyHost", hostEdit->text().trimmed());
                m_settings->setValue("update/proxyPort", portSpin->value());
                m_settings->setValue("update/proxyUsername", userEdit->text());
                m_settings->setValue("update/proxyPassword", passwordEdit->text());
                if (m_updateManager != nullptr) {
                    m_updateManager->reloadNetworkSettings();
                }
                updateSettingsSummary(summaryLabel, btn,
                    proxySummaryForSettings(*m_settings));
            }
        });

        settingsLayout->addWidget(row);
    }

    settingsLayout->addStretch();
    addFooterNode(tr("Settings"), m_ui->settingsPage, m_ui->settingsKey, 0, ElaIconType::GearComplex);
}
