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

#include "CodexBubbleFormatter.hpp"

#include "shijima-qt/AppLog.hpp"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QMouseEvent>

namespace {

constexpr int kCodexMaxBubbleWidth = 360;
constexpr int kCodexMaxBubbleHeight = 240;
constexpr int kCodexMaxTextWidth = 320;
constexpr int kCodexMaxBodyLines = 8;
constexpr int kCodexBaseDurationMs = 8000;

}

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
    QString effectiveTitle = title;
    if (effectiveTitle.isEmpty()) {
        effectiveTitle = QCoreApplication::translate("SpeechBubbleWidget",
            "Codex · Completed");
    }
    Content content { effectiveTitle, text, Tone::CodexReady };
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
    int maxTextWidth = m_currentCodex ? kCodexMaxTextWidth : 200;
    int titleHeight = 0;
    int titleWidth = 0;
    if (m_currentCodex && !m_content.title.isEmpty()) {
        QFont titleFont = font;
        titleFont.setBold(true);
        QFontMetrics titleMetrics(titleFont);
        titleHeight = titleMetrics.lineSpacing() + 4;
        titleWidth = titleMetrics.horizontalAdvance(m_content.title);
    }

    QString body = m_text;
    int effectiveDurationMs = durationMs;
    int maxTextHeight = kCodexMaxBubbleHeight
        - titleHeight
        - m_padding * 2
        - m_tailHeight
        - 4;
    if (m_currentCodex) {
        auto excerpt = formatCodexBubbleExcerpt(m_text, font, maxTextWidth,
            maxTextHeight, kCodexMaxBodyLines);
        body = excerpt.text;
        effectiveDurationMs = codexBubbleDisplayDurationMs(
            excerpt.retainedGraphemes);
    }
    m_content.body = body;
    m_text = body;
    QRect textRect;
    if (m_currentCodex) {
        configureCodexMarkdownDocument(m_codexDocument, body, font,
            maxTextWidth);
        int bodyWidth = qBound(1, qCeil(m_codexDocument.idealWidth()),
            maxTextWidth);
        // idealWidth() is computed before wrapping. Reapply the actual width
        // used by the bubble so headings/lists and code blocks are measured
        // with exactly the same layout that paintEvent() will draw.
        configureCodexMarkdownDocument(m_codexDocument, body, font,
            bodyWidth);
        qreal bodyHeight = m_codexDocument.documentLayout()
            ->documentSize().height();
        if (bodyHeight > maxTextHeight && bodyWidth < maxTextWidth) {
            bodyWidth = maxTextWidth;
            configureCodexMarkdownDocument(m_codexDocument, body, font,
                bodyWidth);
            bodyHeight = m_codexDocument.documentLayout()
                ->documentSize().height();
        }
        textRect = QRect(0, 0, bodyWidth, qCeil(bodyHeight));
    }
    else {
        m_codexDocument.clear();
        textRect = fm.boundingRect(QRect(0, 0, maxTextWidth, 0),
            Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, body);
    }

    int contentWidth = textRect.width();
    if (m_currentCodex) {
        contentWidth = qMax(contentWidth, titleWidth);
    }
    int bubbleWidth = contentWidth + m_padding * 2 + 4;
    int bubbleHeight = textRect.height() + titleHeight + m_padding * 2 +
        m_tailHeight + 4;
    if (m_currentCodex) {
        bubbleWidth = qBound(60, bubbleWidth, kCodexMaxBubbleWidth);
        bubbleHeight = qBound(40 + m_tailHeight, bubbleHeight,
            kCodexMaxBubbleHeight);
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
    m_hideTimer.start(m_currentCodex ? qMax(kCodexBaseDurationMs,
        effectiveDurationMs) : effectiveDurationMs);
}

void SpeechBubbleWidget::showNextCodexBubble() {
    if (m_codexQueue.isEmpty()) {
        hideBubble();
        return;
    }
    auto content = m_codexQueue.dequeue();
    showContent(content, m_anchorScreenPos, kCodexBaseDurationMs);
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
    if (m_currentCodex) {
        // QTextDocument's Markdown parser gives Codex notifications real
        // emphasis, headings, lists and code spans. The document has no
        // interaction flags and all anchor formats are cleared by the
        // formatter, so untrusted text can never launch an external URL.
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = palette;
        context.palette.setColor(QPalette::Text, textColor);
        context.clip = textRect;
        painter.save();
        painter.translate(textRect.topLeft());
        context.clip = QRectF(QPointF(0, 0), textRect.size());
        m_codexDocument.documentLayout()->draw(&painter, context);
        painter.restore();
    }
    else {
        int bodyFlags = Qt::TextWordWrap | Qt::AlignCenter;
        painter.drawText(textRect, bodyFlags, m_text);
    }
}

void SpeechBubbleWidget::mousePressEvent(QMouseEvent *event) {
    if (m_currentCodex && m_active && event != nullptr &&
        event->button() == Qt::LeftButton) {
        emit codexActivated();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

#include "SpeechBubbleWidget.moc"
