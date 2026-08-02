#pragma once

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

#include <QMap>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <QTimer>
#include <QWidget>

class SpeechBubbleWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Tone {
        Normal,
        CodexReady,
        CodexRunning,
        CodexNeedsInput,
        CodexBlocked,
    };
    struct Content {
        QString title;
        QString body;
        Tone tone = Tone::Normal;
    };

    explicit SpeechBubbleWidget(QWidget *parent = nullptr);
    void showBubble(const QString &text, const QPoint &anchorScreenPos);
    void showCodexBubble(const QString &text, const QPoint &anchorScreenPos,
        const QString &title = QStringLiteral("Codex · 已完成"));
    void hideBubble();
    void updatePosition(const QPoint &anchorScreenPos);
    bool isActive() const { return m_active; }

    static QStringList loadBubbleTexts(const QString &mascotPath = QString());
    static QString randomBubbleText(const QString &mascotPath = QString());

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void showContent(Content const& content, QPoint const& anchorScreenPos,
        int durationMs);
    void showNextCodexBubble();

    Content m_content;
    QQueue<Content> m_codexQueue;
    QString m_text;
    // Only Codex notifications use this document.  Ordinary mascot bubbles
    // continue through QPainter::drawText so their centered, three-second
    // behavior remains unchanged.
    QTextDocument m_codexDocument;
    QTimer m_hideTimer;
    QPoint m_anchorScreenPos;
    bool m_active = false;
    bool m_currentCodex = false;
    int m_tailHeight = 12;
    int m_cornerRadius = 12;
    int m_padding = 12;

    static QMap<QString, QStringList> s_bubbleTextsCache;
};
