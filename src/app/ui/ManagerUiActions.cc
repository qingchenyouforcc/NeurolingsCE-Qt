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
#include "shijima-qt/MascotData.hpp"
#include "shijima-qt/ShijimaHttpApi.hpp"
#include "shijima-qt/ShijimaLocalApi.hpp"
#include "../runtime/ManagerRuntimeState.hpp"
#include "ManagerUiState.hpp"
#include "../runtime/ManagerRuntimeHelpers.hpp"
#include "ManagerUiHelpers.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QLibraryInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QProcess>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include "ElaDialog.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaScrollBar.h"
#include "ElaTheme.h"

namespace ShijimaManagerUiInternal {

namespace {

void fitThemedDialog(ElaDialog *dialog, QSize minimumSize,
    QSize maximumSize, qreal screenHeightRatio = 0.82)
{
    if (dialog == nullptr || dialog->layout() == nullptr) {
        return;
    }
    QRect available;
    if (auto *screen = dialog->screen(); screen != nullptr) {
        available = screen->availableGeometry();
    }
    else if (auto *screen = QGuiApplication::primaryScreen(); screen != nullptr) {
        available = screen->availableGeometry();
    }
    if (available.isEmpty()) {
        available = QRect(0, 0, 1280, 720);
    }

    int maxWidth = qMax(minimumSize.width(), qMin(maximumSize.width(),
        available.width() - 48));
    int maxHeight = qMax(minimumSize.height(), qMin(maximumSize.height(),
        qRound(available.height() * screenHeightRatio)));
    dialog->setMinimumSize(minimumSize);
    dialog->setMaximumSize(maxWidth, maxHeight);
    dialog->layout()->activate();
    // ElaDialog starts with a framework default size.  adjustSize() lets the
    // layout participate before we clamp to a screen-safe envelope.
    dialog->adjustSize();
    QSize desired = dialog->layout()->sizeHint();
    // Leave room for ElaDialog's custom app bar, which is a child rather than
    // a layout item.
    desired.rheight() += 48;
    desired.setWidth(qMax(desired.width(), minimumSize.width()));
    desired.setHeight(qMax(desired.height(), minimumSize.height()));
    dialog->resize(qBound(minimumSize.width(), desired.width(), maxWidth),
        qBound(minimumSize.height(), desired.height(), maxHeight));
}

class ThemedPromptDialog final : public ElaDialog {
public:
    ThemedPromptDialog(QWidget *parent, QString const& title,
        QString const& message, bool question, QString const& acceptText,
        QString const& cancelText, bool destructive = false,
        bool modal = true):
        ElaDialog(parent)
    {
        setWindowTitle(title);
        setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
        setIsFixedSize(true);
        setModal(modal);
        setMinimumWidth(360);
        setMaximumWidth(520);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(22, 18, 22, 16);
        root->setSpacing(8);
        auto *heading = new QLabel(title, this);
        QFont headingFont = heading->font();
        headingFont.setPointSizeF(qMax(11.0, headingFont.pointSizeF() + 2.0));
        headingFont.setWeight(QFont::DemiBold);
        heading->setFont(headingFont);
        heading->setWordWrap(true);
        heading->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        root->addWidget(heading);
        auto *body = new QLabel(message, this);
        body->setWordWrap(true);
        body->setTextFormat(Qt::PlainText);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        body->setMaximumWidth(470);
        QWidget *bodySurface = body;
        // Keep short prompts on the dialog surface.  Only unusually long
        // diagnostics receive a scroll container, which keeps confirmations
        // compact and avoids the opaque white rectangle seen in native boxes.
        if (message.size() > 480 || message.count(QLatin1Char('\n')) > 8) {
            auto *bodyScroll = new QScrollArea(this);
            bodyScroll->setObjectName(QStringLiteral("themedPromptScroll"));
            bodyScroll->setFrameShape(QFrame::NoFrame);
            bodyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            bodyScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            bodyScroll->setWidgetResizable(true);
            bodyScroll->setMaximumHeight(180);
            bodyScroll->setVerticalScrollBar(new ElaScrollBar(bodyScroll));
            bodyScroll->setAutoFillBackground(false);
            bodyScroll->viewport()->setAutoFillBackground(false);
            bodyScroll->setWidget(body);
            bodySurface = bodyScroll;
        }
        root->addWidget(bodySurface);

        auto *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 8, 0, 0);
        buttons->setSpacing(8);
        buttons->addStretch();
        if (question) {
            auto *cancel = new ElaPushButton(cancelText.isEmpty()
                ? QCoreApplication::translate("ShijimaManager", "Cancel")
                : cancelText, this);
            cancel->setAutoDefault(false);
            cancel->setDefault(false);
            cancel->setMinimumHeight(38);
            cancel->setFocusPolicy(Qt::StrongFocus);
            connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
            buttons->addWidget(cancel);

            auto *accept = new ElaPushButton(acceptText.isEmpty()
                ? QCoreApplication::translate("ShijimaManager", "OK")
                : acceptText, this);
            accept->setAutoDefault(false);
            accept->setDefault(false);
            accept->setMinimumHeight(38);
            accept->setFocusPolicy(Qt::StrongFocus);
            ElaThemeType::ThemeColor const lightRole = destructive
                ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryNormal;
            ElaThemeType::ThemeColor const darkRole = destructive
                ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryNormal;
            accept->setLightDefaultColor(eTheme->getThemeColor(
                ElaThemeType::Light, lightRole));
            accept->setLightHoverColor(eTheme->getThemeColor(
                ElaThemeType::Light,
                destructive ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryHover));
            accept->setLightPressColor(eTheme->getThemeColor(
                ElaThemeType::Light,
                destructive ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryPress));
            accept->setLightTextColor(ElaThemeColor(
                ElaThemeType::Light, BasicTextInvert));
            accept->setDarkDefaultColor(eTheme->getThemeColor(
                ElaThemeType::Dark, darkRole));
            accept->setDarkHoverColor(eTheme->getThemeColor(
                ElaThemeType::Dark,
                destructive ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryHover));
            accept->setDarkPressColor(eTheme->getThemeColor(
                ElaThemeType::Dark,
                destructive ? ElaThemeType::StatusDanger : ElaThemeType::PrimaryPress));
            accept->setDarkTextColor(ElaThemeColor(
                ElaThemeType::Dark, BasicTextInvert));
            connect(accept, &QPushButton::clicked, this, &QDialog::accept);
            buttons->addWidget(accept);
            cancel->setFocus(Qt::OtherFocusReason);
        }
        else {
            auto *close = new ElaPushButton(
                QCoreApplication::translate("ShijimaManager", "Close"), this);
            close->setAutoDefault(false);
            close->setDefault(false);
            close->setMinimumHeight(38);
            close->setFocusPolicy(Qt::StrongFocus);
            connect(close, &QPushButton::clicked, this, &QDialog::accept);
            buttons->addWidget(close);
            close->setFocus(Qt::OtherFocusReason);
        }
        root->addLayout(buttons);
        connect(this, &ElaDialog::closeButtonClicked, this, &QDialog::reject);

        auto applyPalette = [this, heading, body, bodySurface](
            ElaThemeType::ThemeMode mode) {
            QPalette palette = this->palette();
            palette.setColor(QPalette::Window, ElaThemeColor(mode, DialogBase));
            palette.setColor(QPalette::WindowText, ElaThemeColor(mode, BasicText));
            palette.setColor(QPalette::Text, ElaThemeColor(mode, BasicText));
            palette.setColor(QPalette::Base, ElaThemeColor(mode, DialogBase));
            body->setPalette(palette);
            heading->setPalette(palette);
            bodySurface->setPalette(palette);
            this->setPalette(palette);
        };
        applyPalette(eTheme->getThemeMode());
        connect(eTheme, &ElaTheme::themeModeChanged, this, applyPalette);
        fitThemedDialog(this, QSize(360, 148), QSize(520, 300), 0.78);
    }
};

class ThemedTextInputDialog final : public ElaDialog {
public:
    ThemedTextInputDialog(QWidget *parent, QString const& title,
        QString const& label, QString const& initial,
        QString const& acceptText, QString const& cancelText):
        ElaDialog(parent)
    {
        setWindowTitle(title);
        setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
        setIsFixedSize(true);
        setModal(true);
        setMinimumWidth(360);
        setMaximumWidth(520);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(22, 18, 22, 16);
        root->setSpacing(8);
        auto *heading = new QLabel(title, this);
        QFont headingFont = heading->font();
        headingFont.setPointSizeF(qMax(11.0, headingFont.pointSizeF() + 2.0));
        headingFont.setWeight(QFont::DemiBold);
        heading->setFont(headingFont);
        heading->setWordWrap(true);
        root->addWidget(heading);
        auto *description = new QLabel(label, this);
        description->setWordWrap(true);
        root->addWidget(description);
        input = new ElaLineEdit(this);
        input->setText(initial);
        input->setMinimumHeight(38);
        input->setClearButtonEnabled(true);
        root->addWidget(input);

        auto *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 8, 0, 0);
        buttons->setSpacing(8);
        buttons->addStretch();
        auto *cancel = new ElaPushButton(cancelText.isEmpty()
            ? QCoreApplication::translate("ShijimaManager", "Cancel")
            : cancelText, this);
        cancel->setAutoDefault(false);
        cancel->setDefault(false);
        cancel->setMinimumHeight(38);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(cancel);
        auto *accept = new ElaPushButton(acceptText.isEmpty()
            ? QCoreApplication::translate("ShijimaManager", "Save")
            : acceptText, this);
        accept->setAutoDefault(false);
        accept->setDefault(false);
        accept->setMinimumHeight(38);
        accept->setLightDefaultColor(ElaThemeColor(
            ElaThemeType::Light, PrimaryNormal));
        accept->setLightHoverColor(ElaThemeColor(
            ElaThemeType::Light, PrimaryHover));
        accept->setLightPressColor(ElaThemeColor(
            ElaThemeType::Light, PrimaryPress));
        accept->setLightTextColor(ElaThemeColor(
            ElaThemeType::Light, BasicTextInvert));
        accept->setDarkDefaultColor(ElaThemeColor(
            ElaThemeType::Dark, PrimaryNormal));
        accept->setDarkHoverColor(ElaThemeColor(
            ElaThemeType::Dark, PrimaryHover));
        accept->setDarkPressColor(ElaThemeColor(
            ElaThemeType::Dark, PrimaryPress));
        accept->setDarkTextColor(ElaThemeColor(
            ElaThemeType::Dark, BasicTextInvert));
        connect(accept, &QPushButton::clicked, this, &QDialog::accept);
        buttons->addWidget(accept);
        root->addLayout(buttons);
        connect(input, &QLineEdit::returnPressed, this, &QDialog::accept);
        connect(this, &ElaDialog::closeButtonClicked, this, &QDialog::reject);
        input->setFocus(Qt::OtherFocusReason);
        input->selectAll();

        auto applyPalette = [this, heading, description](
            ElaThemeType::ThemeMode mode) {
            QPalette palette = this->palette();
            palette.setColor(QPalette::Window, ElaThemeColor(mode, DialogBase));
            palette.setColor(QPalette::WindowText, ElaThemeColor(mode, BasicText));
            palette.setColor(QPalette::Text, ElaThemeColor(mode, BasicText));
            palette.setColor(QPalette::Base, ElaThemeColor(mode, DialogBase));
            this->setPalette(palette);
            heading->setPalette(palette);
            description->setPalette(palette);
        };
        applyPalette(eTheme->getThemeMode());
        connect(eTheme, &ElaTheme::themeModeChanged, this, applyPalette);
        fitThemedDialog(this, QSize(360, 156), QSize(520, 280), 0.72);
    }

    QString value() const { return input->text(); }

private:
    ElaLineEdit *input = nullptr;
};

}

bool showThemedQuestion(QWidget *parent, QString const& title,
    QString const& message, QString const& acceptText,
    QString const& cancelText, bool destructive)
{
    ThemedPromptDialog dialog(parent, title, message, true, acceptText,
        cancelText, destructive);
    return dialog.exec() == QDialog::Accepted;
}

bool showThemedTextInput(QWidget *parent, QString const& title,
    QString const& label, QString const& initial, QString *result,
    QString const& acceptText, QString const& cancelText)
{
    if (result == nullptr) {
        return false;
    }
    ThemedTextInputDialog dialog(parent, title, label, initial,
        acceptText, cancelText);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    *result = dialog.value();
    return true;
}

void showThemedInformation(QWidget *parent, QString const& title,
    QString const& message)
{
    ThemedPromptDialog dialog(parent, title, message, false, {}, {});
    dialog.exec();
}

void showThemedWarning(QWidget *parent, QString const& title,
    QString const& message)
{
    ThemedPromptDialog dialog(parent, title, message, false, {}, {});
    dialog.exec();
}

void showThemedInformationAsync(QWidget *parent, QString const& title,
    QString const& message)
{
    auto *dialog = new ThemedPromptDialog(parent, title, message, false,
        {}, {}, false, false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString colorToString(QColor const& color) {
    auto rgb = color.toRgb();
    std::array<char, 8> buf;
    snprintf(&buf[0], buf.size(), "#%02hhX%02hhX%02hhX",
        (uint8_t)rgb.red(), (uint8_t)rgb.green(),
        (uint8_t)rgb.blue());
    buf[buf.size() - 1] = 0;
    return QString { &buf[0] };
}

}

namespace ShijimaManagerRuntimeInternal {

void dispatchToMainThread(std::function<void()> callback) {
    QTimer *timer = new QTimer;
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [timer, callback]() {
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

}

namespace ShijimaManagerUiInternal {

void applyMascotListTheme(QListWidget& listWidget) {
    auto mode = eTheme->getThemeMode();
    QColor bg = eTheme->getThemeColor(mode, ElaThemeType::WindowBase);
    QColor text = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
    QColor bgAlt = eTheme->getThemeColor(mode, ElaThemeType::BasicBase);
    QColor hover = eTheme->getThemeColor(mode, ElaThemeType::BasicHover);
    QColor selected = eTheme->getThemeColor(mode, ElaThemeType::PrimaryNormal);
    QColor selectedText = eTheme->getThemeColor(mode, ElaThemeType::BasicTextInvert);
    QColor border = eTheme->getThemeColor(mode, ElaThemeType::BasicBorder);
    listWidget.setStyleSheet(QString(
        "QListWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  outline: none;"
        "}"
        "QListWidget:focus {"
        "  border: 2px solid %5;"
        "}"
        "QListWidget::item {"
        "  padding: 4px;"
        "  border-radius: 4px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: %4;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: %5;"
        "  color: %8;"
        "}"
        "QScrollBar:vertical {"
        "  background: %6;"
        "  width: 8px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %7;"
        "  min-height: 30px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(bg.name(), text.name(), border.name(),
          hover.name(), selected.name(), bgAlt.name(),
          border.name(), selectedText.name()));
}

}

void ShijimaManager::importAction() {
    // This is the user-facing entry point for archive import from the menu.
    auto paths = QFileDialog::getOpenFileNames(this, tr("Choose shimeji archive..."));
    if (paths.isEmpty()) {
        return;
    }
    importWithDialog(paths);
}

void ShijimaManager::quitAction() {
    m_allowClose = true;
    closeManagerWindow();
}

void ShijimaManager::deleteAction() {
    if (m_runtime->templates.loadedMascots().size() == 0) {
        return;
    }

    // Filter out non-deletable templates before prompting so the confirmation
    // dialog only lists actions that will actually happen.
    auto selected = m_ui->listWidget->selectedItems();
    for (long i = (long)selected.size() - 1; i >= 0; --i) {
        auto mascotData = m_runtime->templates.loadedMascots()[selected[i]->text()];
        if (!mascotData->deletable()) {
            selected.remove(i);
        }
    }
    if (selected.size() == 0) {
        return;
    }

    QString msg = tr("Are you sure you want to delete these shimeji?");
    for (long i = 0; i < selected.size() && i < 5; ++i) {
        msg += "\n* " + selected[i]->text();
    }
    if (selected.size() > 5) {
        msg += tr("\n... and %n other(s)", nullptr, selected.size() - 5);
    }

    if (ShijimaManagerUiInternal::showThemedQuestion(this,
            tr("Delete shimeji"), msg, tr("Delete"), tr("Cancel"), true)) {
        QStringList names;
        for (auto item : selected) {
            names.append(item->text());
        }
        // Deletion is delegated to the runtime one template at a time so any
        // validation or reload side effects stay centralized.
        for (auto const& name : names) {
            QString errorMessage;
            removeMascotTemplate(name, errorMessage);
        }
    }
}

void ShijimaManager::updateSandboxBackground() {
    if (m_ui->sandboxWidget != nullptr) {
        m_ui->sandboxWidget->setStyleSheet("#sandboxWindow { background-color: " +
            ShijimaManagerUiInternal::colorToString(m_ui->sandboxBackground) + "; }");
    }
}

void ShijimaManager::updateStatusBar() {
    if (m_ui->statusLabel == nullptr) {
        return;
    }
    // Keep the status text intentionally terse because it updates on every tick.
    int mascotCount = m_runtime->sessions.size();
    int templateCount = m_runtime->templates.loadedMascots().size();
    m_ui->statusLabel->setText(tr("  Mascots: %1  |  Templates: %2")
        .arg(mascotCount).arg(templateCount));
}

void ShijimaManager::updateSelectedMascotDetails() {
    if (m_ui->mascotNameLabel == nullptr) {
        return;
    }

    bool hasTemplates = m_ui->listWidget != nullptr && m_ui->listWidget->count() > 0;
    const auto& loadedMascots = m_runtime->templates.loadedMascots();
    bool hasImportedTemplates = false;
    for (auto *templateData : loadedMascots) {
        if (templateData != nullptr && templateData->deletable()) {
            hasImportedTemplates = true;
            break;
        }
    }
    if (m_ui->mascotEmptyStateWidget != nullptr) {
        m_ui->mascotEmptyStateWidget->setVisible(!hasImportedTemplates);
    }
    if (m_ui->listWidget != nullptr) {
        m_ui->listWidget->setVisible(hasImportedTemplates);
    }
    if (m_ui->spawnRandomButton != nullptr) {
        m_ui->spawnRandomButton->setEnabled(hasTemplates);
    }

    MascotData *data = nullptr;
    auto selected = hasImportedTemplates
        ? m_ui->listWidget->selectedItems() : QList<QListWidgetItem *> {};
    bool canDelete = false;
    for (auto *item : selected) {
        auto *selectedData = loadedMascots.value(item->text(), nullptr);
        if (selectedData != nullptr && selectedData->deletable()) {
            canDelete = true;
            break;
        }
    }
    if (m_ui->deleteMascotButton != nullptr) {
        m_ui->deleteMascotButton->setEnabled(canDelete);
    }
    if (!selected.isEmpty()) {
        data = loadedMascots.value(selected.first()->text(), nullptr);
    }

    if (data == nullptr) {
        // Empty selection should read like a neutral placeholder rather than an
        // error state.
        if (m_ui->mascotPreviewLabel != nullptr) {
            m_ui->mascotPreviewLabel->clear();
            m_ui->mascotPreviewLabel->setText(QStringLiteral("-"));
        }
        m_ui->mascotNameLabel->setText(tr("No mascot selected"));
        m_ui->mascotVersionLabel->clear();
        m_ui->mascotAuthorLabel->clear();
        m_ui->mascotDescriptionLabel->clear();
        return;
    }

    auto const& metadata = data->metadata();
    if (m_ui->mascotPreviewLabel != nullptr) {
        auto pixmap = data->preview().pixmap(80, 80);
        m_ui->mascotPreviewLabel->setText(QString());
        m_ui->mascotPreviewLabel->setPixmap(pixmap);
    }
    m_ui->mascotNameLabel->setText(metadata.name);
    m_ui->mascotVersionLabel->setText(metadata.version.isEmpty()
        ? QString() : tr("Version: %1").arg(metadata.version));
    m_ui->mascotAuthorLabel->setText(metadata.author.isEmpty()
        ? QString() : tr("Author: %1").arg(metadata.author));
    m_ui->mascotDescriptionLabel->setText(metadata.description);
}

void ShijimaManager::itemDoubleClicked(QListWidgetItem *qItem) {
    spawn(qItem->text().toStdString());
}

void ShijimaManager::askClose() {
    // On desktop platforms this is used both for explicit quit and for tray
    // close behavior when the manager is being hidden instead of destroyed.
    setManagerVisible(true);
    if (ShijimaManagerUiInternal::showThemedQuestion(this,
            tr("Close NeurolingsCE"),
            tr("Do you want to close NeurolingsCE?"), tr("Close"),
            tr("Keep open"))) {
#if defined(__APPLE__)
        QCoreApplication::quit();
#else
        m_allowClose = true;
        closeManagerWindow();
#endif
    }
}

void ShijimaManager::setManagerVisible(bool visible) {
    if (visible) {
        // Restore from minimized state before raising so the window actually
        // comes back to the foreground.
        if (isMinimized()) {
            setWindowState(windowState() & ~Qt::WindowMinimized);
        }
        show();
        raise();
        if (window() != nullptr) {
            window()->raise();
            window()->activateWindow();
        }
        m_wasVisible = true;
    }
    else if (m_wasVisible && !visible) {
#if defined(__APPLE__)
        if (m_runtime->sessions.empty()) {
            // macOS prefers to ask before hiding the final visible window.
            askClose();
            return;
        }
#endif
        if (isMinimized()) {
            setWindowState(windowState() & ~Qt::WindowMinimized);
        }
        hide();
        clearFocus();
        m_wasVisible = false;
    }
}

void ShijimaManager::switchLanguage(const QString &langCode) {
    if (langCode == m_ui->currentLanguage) {
        return;
    }

    // Tear down old translators first so the new locale can retranslate from a
    // clean slate.
    if (m_ui->translator != nullptr) {
        qApp->removeTranslator(m_ui->translator);
        delete m_ui->translator;
        m_ui->translator = nullptr;
    }
    if (m_ui->qtTranslator != nullptr) {
        qApp->removeTranslator(m_ui->qtTranslator);
        delete m_ui->qtTranslator;
        m_ui->qtTranslator = nullptr;
    }

    m_ui->currentLanguage = langCode;
    m_settings->setValue("language", langCode);
    if (langCode != "en") {
        m_ui->translator = new QTranslator(this);
        if (m_ui->translator->load("shijima-qt_" + langCode, ":/i18n")) {
            qApp->installTranslator(m_ui->translator);
        }
        m_ui->qtTranslator = new QTranslator(this);
        // Qt's standard-dialog strings live beside the Qt installation, not
        // in NeurolingsCE's embedded resource collection.  Loading from the
        // documented translations path also keeps the translator lifetime
        // tied to this manager while preserving restart-on-language-change.
        if (m_ui->qtTranslator->load("qt_" + langCode,
                QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            qApp->installTranslator(m_ui->qtTranslator);
        }
    }

    if (!m_constructing) {
        // Language changes require a restart because the UI is heavily
        // constructed once and not all widgets retranslate live.
        ShijimaManagerUiInternal::showThemedInformation(this,
            tr("Language Changed"),
            tr("The application will restart to apply the new language."));

        m_localApi->stop();
        m_httpApi->stop();

        const QString program = QCoreApplication::applicationFilePath();
        const QStringList args = QCoreApplication::arguments().mid(1);
        QProcess::startDetached(program, args);
        m_allowClose = true;
        closeManagerWindow();
    }
}

void ShijimaManager::closeManagerWindow() {
#if defined(_WIN32)
    // Fade out on Windows to make hide/quit transitions feel deliberate.
    auto *animation = new QPropertyAnimation(this, "windowOpacity", this);
    connect(animation, &QPropertyAnimation::finished, this, &QWidget::close);
    animation->setDuration(250);
    animation->setStartValue(windowOpacity());
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::InOutSine);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
#else
    ElaWindow::closeWindow();
#endif
}

void ShijimaManager::retranslateUi() {
    setWindowTitle(tr(APP_NAME " \u2014 Mascot Manager"));
    updateStatusBar();
    ShijimaManagerUiInternal::refreshTrayMenu(m_ui->trayController.get());
}
