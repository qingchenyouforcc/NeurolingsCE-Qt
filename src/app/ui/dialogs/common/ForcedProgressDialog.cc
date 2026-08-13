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

#include "shijima-qt/ui/dialogs/common/ForcedProgressDialog.hpp"
#include <QColor>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaTheme.h"

namespace {

QRect forcedProgressAvailableGeometry(QWidget *widget)
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

void sizeForcedProgressDialog(QDialog *dialog, QLayout *layout)
{
    layout->activate();
    QRect available = forcedProgressAvailableGeometry(dialog);
    int maxWidth = qMax(1, qMin(520, available.width() - 32));
    int maxHeight = qMax(1, available.height() - 32);
    int minWidth = qMin(320, maxWidth);
    int minHeight = qMin(144, maxHeight);
    QSize hint = layout->sizeHint();
    QSize preferred(
        qBound(minWidth, qMax(minWidth, hint.width()), maxWidth),
        qBound(minHeight, qMax(minHeight, hint.height()), maxHeight));
    dialog->setMinimumSize(minWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    dialog->resize(preferred);
}

}

ForcedProgressDialog::ForcedProgressDialog(QWidget *parent):
    QDialog(parent)
{
    setObjectName(QStringLiteral("ForcedProgressDialog"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("ForcedProgressLabel"));
    m_label->setWordWrap(true);
    m_label->setMinimumHeight(20);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(m_label);

    m_progressBar = new ElaProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("ForcedProgressBar"));
    m_progressBar->setRange(m_minimum, m_maximum);
    m_progressBar->setAccessibleName(tr("Progress"));
    layout->addWidget(m_progressBar);

    m_buttonLayout = new QHBoxLayout;
    m_buttonLayout->setContentsMargins(0, 2, 0, 0);
    m_buttonLayout->addStretch();
    auto *defaultCancelButton = new ElaPushButton(tr("Cancel"), this);
    defaultCancelButton->setObjectName(QStringLiteral("ForcedProgressCancelButton"));
    defaultCancelButton->setFocusPolicy(Qt::StrongFocus);
    defaultCancelButton->setAutoDefault(false);
    defaultCancelButton->setDefault(false);
    defaultCancelButton->setMinimumHeight(34);
    defaultCancelButton->setAccessibleName(tr("Cancel"));
    m_cancelButton = defaultCancelButton;
    m_ownsCancelButton = true;
    m_buttonLayout->addWidget(m_cancelButton);
    layout->addLayout(m_buttonLayout);
    connect(m_cancelButton, &QPushButton::clicked, this, &ForcedProgressDialog::cancel);

    auto applyTheme = [this]() {
        auto mode = eTheme->getThemeMode();
        QColor surface = ElaThemeColor(mode, DialogBase);
        QColor field = ElaThemeColor(mode, BasicBase);
        QColor border = ElaThemeColor(mode, BasicBorder);
        QColor text = ElaThemeColor(mode, BasicText);
        QColor disabled = ElaThemeColor(mode, BasicTextDisable);
        QColor accent = ElaThemeColor(mode, PrimaryNormal);
        QColor hover = ElaThemeColor(mode, PrimaryHover);
        QColor press = ElaThemeColor(mode, PrimaryPress);
        setStyleSheet(QString(
            "#ForcedProgressDialog { background-color: %1; color: %4; }"
            "#ForcedProgressDialog QLabel { color: %4; background: transparent; }"
            "#ForcedProgressDialog ElaProgressBar, #ForcedProgressDialog QProgressBar { background-color: %2;"
            " color: %4; border: 1px solid %3; border-radius: 7px;"
            " min-height: 12px; }"
            "#ForcedProgressDialog ElaProgressBar::chunk, #ForcedProgressDialog QProgressBar::chunk {"
            " background-color: %5; border-radius: 6px; }"
            "#ForcedProgressDialog QPushButton { background-color: %2;"
            " color: %4; border: 1px solid %3; border-radius: 7px;"
            " padding: 6px 14px; min-height: 30px; }"
            "#ForcedProgressDialog QPushButton:hover { background-color: %6; }"
            "#ForcedProgressDialog QPushButton:pressed { background-color: %7; }"
            "#ForcedProgressDialog QPushButton:focus { border: 2px solid %5; }"
            "#ForcedProgressDialog QPushButton:disabled { background-color: %8;"
            " color: %9; }"
        ).arg(surface.name(QColor::HexArgb), field.name(QColor::HexArgb),
            border.name(QColor::HexArgb), text.name(QColor::HexArgb),
            accent.name(QColor::HexArgb), hover.name(QColor::HexArgb),
            press.name(QColor::HexArgb),
            ElaThemeColor(mode, BasicDisable).name(QColor::HexArgb),
            disabled.name(QColor::HexArgb)));

        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, surface);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, field);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, text);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        this->setPalette(palette);
        if (m_label) {
            m_label->setPalette(palette);
        }
        if (m_progressBar) {
            m_progressBar->setPalette(palette);
        }
        if (m_cancelButton) {
            m_cancelButton->setPalette(palette);
        }
    };
    applyTheme();
    connect(eTheme, &ElaTheme::themeModeChanged, this, applyTheme);
    sizeForcedProgressDialog(this, layout);
}

void ForcedProgressDialog::setRange(int minimum, int maximum) {
    m_minimum = minimum;
    m_maximum = maximum;
    if (m_progressBar) {
        m_progressBar->setRange(minimum, maximum);
    }
    if (m_value < minimum || m_value > maximum) {
        setValue(minimum);
    }
}

void ForcedProgressDialog::setMinimum(int minimum) {
    setRange(minimum, m_maximum);
}

void ForcedProgressDialog::setMaximum(int maximum) {
    setRange(m_minimum, maximum);
}

void ForcedProgressDialog::setValue(int value) {
    const bool changed = m_value != value;
    m_value = value;
    if (m_progressBar) {
        m_progressBar->setValue(value);
    }
    if (changed) {
        Q_EMIT valueChanged(value);
    }
    if (m_autoClose && m_maximum > m_minimum && value >= m_maximum) {
        close();
    }
}

void ForcedProgressDialog::setLabelText(QString const &text) {
    if (m_label) {
        m_label->setText(text);
        if (layout() != nullptr) {
            layout()->activate();
            int targetHeight = qBound(minimumHeight(), sizeHint().height(),
                maximumHeight());
            if (targetHeight > height()) {
                resize(width(), targetHeight);
            }
        }
    }
}

QString ForcedProgressDialog::labelText() const {
    return m_label ? m_label->text() : QString();
}

void ForcedProgressDialog::setCancelButton(QPushButton *button) {
    if (button == m_cancelButton) {
        return;
    }

    if (m_cancelButton) {
        disconnect(m_cancelButton, &QPushButton::clicked,
            this, &ForcedProgressDialog::cancel);
        if (m_buttonLayout) {
            m_buttonLayout->removeWidget(m_cancelButton);
        }
        if (m_ownsCancelButton) {
            m_cancelButton->deleteLater();
        }
        else {
            m_cancelButton->setParent(nullptr);
            m_cancelButton->hide();
        }
    }

    m_cancelButton = button;
    m_ownsCancelButton = false;
    if (!m_cancelButton) {
        return;
    }

    m_cancelButton->setParent(this);
    m_cancelButton->setFocusPolicy(Qt::StrongFocus);
    m_cancelButton->setAutoDefault(false);
    m_cancelButton->setDefault(false);
    m_cancelButton->setMinimumHeight(34);
    m_cancelButton->setAccessibleName(m_cancelButton->text());
    m_cancelButton->setPalette(palette());
    if (m_buttonLayout) {
        m_buttonLayout->addWidget(m_cancelButton);
    }
    connect(m_cancelButton, &QPushButton::clicked, this, &ForcedProgressDialog::cancel);
}

void ForcedProgressDialog::cancel() {
    if (m_wasCanceled) {
        return;
    }
    m_wasCanceled = true;
    Q_EMIT canceled();
}

void ForcedProgressDialog::reset() {
    m_wasCanceled = false;
    m_value = m_minimum;
    if (m_progressBar) {
        m_progressBar->setValue(m_value);
    }
}

void ForcedProgressDialog::closeEvent(QCloseEvent *event) {
    if (!m_allowsClose) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void ForcedProgressDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && !m_allowsClose) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool ForcedProgressDialog::close() {
    m_allowsClose = true;
    return QDialog::close();
}
