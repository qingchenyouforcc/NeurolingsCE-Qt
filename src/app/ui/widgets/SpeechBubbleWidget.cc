// 
// NeurolingsCE - Cross-platform shimeji simulation app for desktop
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

#include "shijima-qt/ui/widgets/SpeechBubbleWidget.hpp"

#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/CodexActivity.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

SpeechBubbleWidget::SpeechBubbleWidget(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint
        | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this]() {
        if (!m_codexQueue.isEmpty()) {
            m_active = false;
            hide();
            showNextCodexBubble();
        }
        else {
            hideBubble();
        }
    });
}

void SpeechBubbleWidget::showBubble(const QString &text, const QPoint &anchorScreenPos) {
    showContent(Content { {}, text, Tone::Normal }, anchorScreenPos, 3000);
}

void SpeechBubbleWidget::showCodexBubble(const QString &text,
    const QPoint &anchorScreenPos, const QString &title)
{
    Content content { title, text, Tone::CodexReady };
    if (m_currentCodex || m_active) {
        if (m_codexQueue.size() >= 8) {
            m_codexQueue.dequeue();
            APP_LOG_WARN("codex") << "Codex bubble queue full; dropped oldest pending notification"
                << " pending_after_drop=" << m_codexQueue.size();
        }
        m_codexQueue.enqueue(content);
        m_anchorScreenPos = anchorScreenPos;
        return;
    }
    m_codexQueue.enqueue(content);
    m_anchorScreenPos = anchorScreenPos;
    showNextCodexBubble();
}

void SpeechBubbleWidget::hideBubble() {
    m_active = false;
    m_currentCodex = false;
    m_hideTimer.stop();
    hide();
}

void SpeechBubbleWidget::showContent(Content const& content,
    QPoint const& anchorScreenPos, int durationMs)
{
    m_hideTimer.stop();
    m_content = content;
    m_text = content.body;
    m_currentCodex = content.tone != Tone::Normal;
    m_active = true;
    m_anchorScreenPos = anchorScreenPos;

    QFont font = QGuiApplication::font();
    if (font.pointSizeF() <= 0) {
        font.setPixelSize(13);
    }
    QFontMetrics fm(font);
    int maxTextWidth = m_currentCodex ? 320 : 200;
    QString body = m_currentCodex
        ? fitCodexText(m_text, font, maxTextWidth, 8)
        : m_text;
    m_content.body = body;
    m_text = body;
    QRect textRect = fm.boundingRect(QRect(0, 0, maxTextWidth, 0),
        Qt::TextWordWrap | Qt::AlignLeft, body);
    int titleHeight = 0;
    if (m_currentCodex && !m_content.title.isEmpty()) {
        QFont titleFont = font;
        titleFont.setBold(true);
        titleHeight = QFontMetrics(titleFont).lineSpacing() + 4;
    }

    int bubbleWidth = textRect.width() + m_padding * 2 + 4;
    int bubbleHeight = textRect.height() + titleHeight + m_padding * 2 +
        m_tailHeight + 4;
    if (m_currentCodex) {
        bubbleWidth = qBound(60, bubbleWidth, 360);
        bubbleHeight = qBound(40 + m_tailHeight, bubbleHeight, 240);
    }
    else {
        // Keep the pre-existing random-bubble sizing behavior unchanged.
        bubbleWidth = qMax(60, bubbleWidth);
        bubbleHeight = qMax(40 + m_tailHeight, bubbleHeight);
    }
    setFixedSize(bubbleWidth, bubbleHeight);
    updatePosition(anchorScreenPos);
    show();
    raise();
    m_hideTimer.start(durationMs);
}

void SpeechBubbleWidget::showNextCodexBubble() {
    if (m_codexQueue.isEmpty()) {
        hideBubble();
        return;
    }
    auto content = m_codexQueue.dequeue();
    showContent(content, m_anchorScreenPos, 8000);
}

QString SpeechBubbleWidget::fitCodexText(QString const& text,
    QFont const& font, int width, int maxLines)
{
    QString candidate = truncateCodexGraphemes(text, 4096);
    QFontMetrics fm(font);
    while (!candidate.isEmpty()) {
        auto rect = fm.boundingRect(QRect(0, 0, width, 0),
            Qt::TextWordWrap | Qt::AlignLeft, candidate);
        if (rect.height() <= maxLines * fm.lineSpacing()) {
            return candidate;
        }
        candidate = truncateCodexGraphemes(candidate,
            qMax(1, candidate.size() - 8));
        if (candidate.endsWith(QLatin1Char('…'))) {
            candidate.chop(1);
        }
    }
    return QStringLiteral("…");
}

void SpeechBubbleWidget::updatePosition(const QPoint &anchorScreenPos) {
    m_anchorScreenPos = anchorScreenPos;
    if (!m_active) return;

    int bubbleWidth = width();
    int bubbleHeight = height();

    // Position bubble above the mascot's anchor point
    int x = anchorScreenPos.x() - bubbleWidth / 2;
    int y = anchorScreenPos.y() - bubbleHeight - 8;

    // Ensure bubble stays on screen
    QScreen *screen = QGuiApplication::screenAt(anchorScreenPos);
    if (screen) {
        QRect screenGeom = screen->availableGeometry();
        if (x < screenGeom.left()) x = screenGeom.left();
        if (x + bubbleWidth > screenGeom.right())
            x = screenGeom.right() - bubbleWidth;
        if (y < screenGeom.top()) {
            // Show below the mascot instead
            y = anchorScreenPos.y() + 16;
        }
    }

    move(x, y);
}

void SpeechBubbleWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont font = QGuiApplication::font();
    if (font.pointSizeF() <= 0) {
        font.setPixelSize(13);
    }
    painter.setFont(font);

    // Bubble body rect (above the tail)
    QRectF bodyRect(2, 2, width() - 4, height() - m_tailHeight - 4);

    // Draw bubble body with rounded corners
    QPainterPath bubblePath;
    bubblePath.addRoundedRect(bodyRect, m_cornerRadius, m_cornerRadius);

    // Draw tail (small triangle at bottom center)
    QPointF tailLeft(width() / 2.0 - 8, bodyRect.bottom());
    QPointF tailRight(width() / 2.0 + 8, bodyRect.bottom());
    QPointF tailTip(width() / 2.0, height() - 3.0);

    QPainterPath tailPath;
    tailPath.moveTo(tailLeft);
    tailPath.lineTo(tailTip);
    tailPath.lineTo(tailRight);
    tailPath.closeSubpath();

    QPainterPath fullPath = bubblePath.united(tailPath);

    QPainterPath shadowPath = fullPath.translated(0, 2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 32, 36, 38));
    painter.drawPath(shadowPath);

    // Draw text using the current system palette so light/dark mode and high
    // contrast themes remain legible without hard-coded white/black colors.
    auto palette = QGuiApplication::palette();
    QColor base = palette.color(QPalette::Base);
    base.setAlpha(235);
    QColor textColor = palette.color(QPalette::Text);
    QColor border = palette.color(QPalette::Mid);
    if (m_currentCodex) {
        switch (m_content.tone) {
            case Tone::CodexReady:
                border = palette.color(QPalette::Highlight);
                break;
            case Tone::CodexRunning:
                border = palette.color(QPalette::Link);
                break;
            case Tone::CodexNeedsInput:
                border = palette.color(QPalette::BrightText);
                break;
            case Tone::CodexBlocked:
                border = palette.color(QPalette::Dark);
                break;
            case Tone::Normal:
                break;
        }
    }
    border.setAlpha(200);
    painter.setPen(QPen(border, 1.2));
    painter.setBrush(base);
    painter.drawPath(fullPath);

    QRectF textRect = bodyRect.adjusted(m_padding, m_padding,
        -m_padding, -m_padding);
    painter.setPen(textColor);
    if (m_currentCodex && !m_content.title.isEmpty()) {
        QFont titleFont = font;
        titleFont.setBold(true);
        painter.setFont(titleFont);
        QRectF titleRect = textRect;
        titleRect.setHeight(QFontMetrics(titleFont).lineSpacing());
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
            m_content.title);
        painter.setFont(font);
        textRect.setTop(titleRect.bottom() + 3);
    }
    painter.drawText(textRect, Qt::TextWordWrap | Qt::AlignCenter, m_text);
}

#include "SpeechBubbleWidget.moc"
