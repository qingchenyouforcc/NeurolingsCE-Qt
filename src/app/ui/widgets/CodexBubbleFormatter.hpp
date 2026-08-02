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

#include <QFont>
#include <QTextDocument>
#include <QString>

struct CodexBubbleExcerpt {
    // The excerpt is a sanitized Markdown source string.  SpeechBubbleWidget
    // renders it with QTextDocument; keeping the source here means that
    // emphasis, lists and code spans survive the normal (non-truncated) path.
    QString text;
    int retainedGraphemes = 0;
    bool truncated = false;
};

// Escape raw HTML and remove Markdown link destinations before handing
// untrusted Codex text to Qt's rich-text parser.  The operation is
// intentionally idempotent so callers can safely pass an already-sanitized
// excerpt back through the helper.
QString sanitizeCodexMarkdown(QString const& source);

// Return the text that a sanitized Markdown document visibly contains.  This
// is used for duration accounting and tests; formatting markers are not
// counted as visible graphemes.
QString codexMarkdownPlainText(QString const& markdown);

// Configure a QTextDocument for safe, non-interactive Codex rendering.
// setMarkdown() itself never opens links, and this helper additionally clears
// anchor formats so a future mouse/event path cannot accidentally make a
// notification an external-link launcher.
void configureCodexMarkdownDocument(QTextDocument &document,
    QString const& markdown, QFont const& font, int textWidth);

bool codexMarkdownFits(QString const& markdown, QFont const& font,
    int textWidth, int maxTextHeight, int maxLines);

CodexBubbleExcerpt formatCodexBubbleExcerpt(QString const& source,
    QFont const& font, int textWidth, int maxTextHeight, int maxLines);

int codexBubbleDisplayDurationMs(int visibleGraphemes);
