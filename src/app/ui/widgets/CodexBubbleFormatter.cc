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

#include "CodexBubbleFormatter.hpp"

#include "shijima-qt/CodexActivity.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextBoundaryFinder>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QRect>
#include <QStringList>
#include <QVector>

#include <utility>

namespace {

constexpr int kBaseDurationMs = 8000;
constexpr int kMaxDurationMs = 12000;
constexpr int kDurationGraphemeThreshold = 80;
constexpr int kDurationMsPerGrapheme = 25;

QString stripMarkdownLinkDestinations(QString text) {
    // Markdown links are not useful in a transient notification and their
    // destinations are untrusted input. Keep the visible label while dropping
    // the destination. A scanner (rather than a broad regex) handles escaped
    // and nested parentheses without consuming the rest of the message.
    QString result;
    result.reserve(text.size());
    int cursor = 0;
    while (cursor < text.size()) {
        int open = text.indexOf(QChar('['), cursor);
        if (open < 0) {
            result += text.mid(cursor);
            break;
        }

        int labelStart = open + 1;
        bool image = open > 0 && text.at(open - 1) == QChar('!');
        int close = labelStart;
        bool escaped = false;
        for (; close < text.size(); ++close) {
            QChar ch = text.at(close);
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == QChar('\\')) {
                escaped = true;
                continue;
            }
            if (ch == QChar(']')) {
                break;
            }
            if (ch == QChar('\n') || ch == QChar('\r')) {
                close = text.size();
                break;
            }
        }
        if (close >= text.size() || close + 1 >= text.size() ||
            text.at(close + 1) != QChar('('))
        {
            // Not an inline link. Copy the opening bracket and keep scanning
            // after it, so ordinary bracketed prose remains unchanged.
            result += text.mid(cursor, open - cursor + 1);
            cursor = open + 1;
            continue;
        }

        int depth = 1;
        escaped = false;
        int end = close + 2;
        for (; end < text.size(); ++end) {
            QChar ch = text.at(end);
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == QChar('\\')) {
                escaped = true;
                continue;
            }
            if (ch == QChar('(')) {
                ++depth;
            }
            else if (ch == QChar(')')) {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
            else if (ch == QChar('\n') || ch == QChar('\r')) {
                end = text.size();
                break;
            }
        }
        if (end >= text.size() || depth != 0) {
            result += text.mid(cursor, open - cursor + 1);
            cursor = open + 1;
            continue;
        }

        int prefixEnd = image && open > cursor ? open - 1 : open;
        result += text.mid(cursor, prefixEnd - cursor);
        result += text.mid(labelStart, close - labelStart);
        cursor = end + 1;
    }
    return result;
}

QString removeReferenceDefinitions(QString text) {
    // A reference definition can create an anchor even when the destination
    // is not next to its label. Drop those definitions; unresolved
    // `[label][id]` syntax is then rendered as harmless literal text.
    static QRegularExpression const definition(
        QStringLiteral("(?m)^\\s{0,3}\\[[^\\]\\r\\n]+\\]:[^\\r\\n]*$")
    );
    text.replace(definition, QString());
    return text;
}

QString escapeRawHtml(QString const& text) {
    QString result;
    result.reserve(text.size());
    for (QChar const ch : text) {
        if (ch == QChar('<')) {
            result += QStringLiteral("&lt;");
        }
        else if (ch == QChar('>')) {
            result += QStringLiteral("&gt;");
        }
        else {
            result += ch;
        }
    }
    return result;
}

QString preserveMarkdownLineBreaks(QString text) {
    // QTextDocument follows CommonMark's soft-break rule for a single
    // newline and would otherwise collapse the explicit line boundaries that
    // the legacy bubble formatter measured. Two trailing spaces are the
    // Markdown hard-break spelling; add them only to non-empty lines and keep
    // blank lines/paragraphs untouched.
    QStringList lines = text.split(QChar('\n'));
    for (int i = 0; i + 1 < lines.size(); ++i) {
        if (!lines.at(i).isEmpty() && !lines.at(i).endsWith(QStringLiteral("  "))) {
            lines[i] += QStringLiteral("  ");
        }
    }
    return lines.join(QChar('\n'));
}

QVector<int> graphemeBoundaries(QString const& text) {
    QVector<int> boundaries { 0 };
    if (text.isEmpty()) {
        return boundaries;
    }

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    while (true) {
        int boundary = finder.toNextBoundary();
        if (boundary < 0 || boundary <= boundaries.constLast()) {
            break;
        }
        boundaries.append(boundary);
    }
    if (boundaries.constLast() != text.size()) {
        boundaries.append(text.size());
    }
    return boundaries;
}

QString makeHeadTailCandidate(QString const& text,
    QVector<int> const& boundaries, int retainedGraphemes,
    bool separateMarker)
{
    int totalGraphemes = boundaries.size() - 1;
    if (retainedGraphemes <= 0 || totalGraphemes <= 0) {
        return {};
    }
    if (retainedGraphemes >= totalGraphemes) {
        return text;
    }

    if (retainedGraphemes == 1) {
        return text.left(boundaries.at(1)) + QStringLiteral("…");
    }

    int headCount = qMax(1, static_cast<int>(
        (static_cast<qint64>(retainedGraphemes) * 4 + 6) / 7));
    headCount = qMin(headCount, retainedGraphemes - 1);
    int tailCount = retainedGraphemes - headCount;
    tailCount = qMin(tailCount, totalGraphemes - headCount);
    headCount = qMin(headCount, totalGraphemes - tailCount);

    QString head = text.left(boundaries.at(headCount)).trimmed();
    QString tail = text.mid(boundaries.at(totalGraphemes - tailCount)).trimmed();
    if (tail.isEmpty()) {
        return head + QStringLiteral("…");
    }
    return separateMarker
        ? head + QStringLiteral("\n…\n") + tail
        : head + QStringLiteral("…") + tail;
}

struct SearchResult {
    QString text;
    int retainedGraphemes = 0;
};

SearchResult findBestCandidate(QString const& text,
    QVector<int> const& boundaries, QFont const& font,
    int width, int maxHeight, int maxLines, bool separateMarker)
{
    int totalGraphemes = boundaries.size() - 1;
    int low = separateMarker ? 2 : 1;
    int high = qMax(1, totalGraphemes - 1);
    SearchResult best;

    while (low <= high) {
        int candidateBudget = low + (high - low) / 2;
        QString candidate = makeHeadTailCandidate(text, boundaries,
            candidateBudget, separateMarker);
        if (codexMarkdownFits(candidate, font, width, maxHeight, maxLines)) {
            best.text = std::move(candidate);
            best.retainedGraphemes = candidateBudget;
            low = candidateBudget + 1;
        }
        else {
            high = candidateBudget - 1;
        }
    }
    return best;
}

SearchResult findBestPrefix(QString const& text,
    QVector<int> const& boundaries, QFont const& font,
    int width, int maxHeight, int maxLines)
{
    int totalGraphemes = boundaries.size() - 1;
    int low = 1;
    int high = qMax(1, totalGraphemes - 1);
    SearchResult best;

    while (low <= high) {
        int candidateBudget = low + (high - low) / 2;
        QString candidate = text.left(boundaries.at(candidateBudget)).trimmed()
            + QStringLiteral("…");
        if (codexMarkdownFits(candidate, font, width, maxHeight, maxLines)) {
            best.text = std::move(candidate);
            best.retainedGraphemes = candidateBudget;
            low = candidateBudget + 1;
        }
        else {
            high = candidateBudget - 1;
        }
    }
    return best;
}

void clearCodexAnchors(QTextDocument &document) {
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        QVector<QPair<int, int>> anchors;
        for (QTextBlock::Iterator iterator = block.begin();
            iterator != block.end(); ++iterator)
        {
            QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            QTextCharFormat format = fragment.charFormat();
            if (!format.isAnchor() &&
                !format.hasProperty(QTextFormat::AnchorHref) &&
                !format.hasProperty(QTextFormat::AnchorName))
            {
                continue;
            }
            anchors.append(qMakePair(fragment.position(), fragment.length()));
        }
        for (auto const& anchor : anchors) {
            QTextCursor cursor(&document);
            cursor.setPosition(anchor.first);
            cursor.setPosition(anchor.first + anchor.second,
                QTextCursor::KeepAnchor);
            QTextCharFormat format = cursor.charFormat();
            format.clearProperty(QTextFormat::AnchorHref);
            format.clearProperty(QTextFormat::AnchorName);
            format.setAnchor(false);
            format.setUnderlineStyle(QTextCharFormat::NoUnderline);
            cursor.mergeCharFormat(format);
        }
    }
}

int documentLineCount(QTextDocument const& document) {
    int lines = 0;
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        auto *layout = block.layout();
        lines += layout != nullptr ? qMax(1, layout->lineCount()) : 1;
    }
    return lines;
}

}

QString sanitizeCodexMarkdown(QString const& source) {
    QString normalized = normalizeCodexBubbleText(source);
    normalized = stripMarkdownLinkDestinations(normalized);
    normalized = removeReferenceDefinitions(normalized);
    return escapeRawHtml(normalized);
}

void configureCodexMarkdownDocument(QTextDocument &document,
    QString const& markdown, QFont const& font, int textWidth)
{
    document.clear();
    document.setUndoRedoEnabled(false);
    document.setDocumentMargin(0);
    document.setDefaultFont(font);
    document.setMarkdown(preserveMarkdownLineBreaks(
            sanitizeCodexMarkdown(markdown)),
        QTextDocument::MarkdownDialectGitHub);
    document.setDefaultFont(font);
    document.setDocumentMargin(0);
    document.setTextWidth(qMax(1, textWidth));
    clearCodexAnchors(document);
}

QString codexMarkdownPlainText(QString const& markdown) {
    QTextDocument document;
    QFont font;
    configureCodexMarkdownDocument(document, markdown, font, 100000);
    return document.toPlainText();
}

bool codexMarkdownFits(QString const& markdown, QFont const& font,
    int textWidth, int maxTextHeight, int maxLines)
{
    if (maxTextHeight <= 0 || maxLines <= 0) {
        return false;
    }
    QTextDocument document;
    configureCodexMarkdownDocument(document, markdown, font, textWidth);
    qreal height = document.documentLayout()->documentSize().height();
    if (height > static_cast<qreal>(maxTextHeight) + 0.5 ||
        documentLineCount(document) > maxLines)
    {
        return false;
    }

    // Keep the old QPainter::drawText measurement as a conservative second
    // budget. This prevents a Markdown paragraph margin or list indent from
    // producing an excerpt that would violate the established 8-line bubble
    // contract when compared with the legacy plain-text path.
    QFontMetrics metrics(font);
    QRect plainRect = metrics.boundingRect(QRect(0, 0, qMax(1, textWidth), 0),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
        document.toPlainText());
    return plainRect.height() <= maxTextHeight;
}

CodexBubbleExcerpt formatCodexBubbleExcerpt(QString const& source,
    QFont const& font, int textWidth, int maxTextHeight, int maxLines)
{
    CodexBubbleExcerpt result;
    QString normalized = sanitizeCodexMarkdown(source);
    if (normalized.isEmpty()) {
        return result;
    }

    auto boundaries = graphemeBoundaries(normalized);
    if (codexMarkdownFits(normalized, font, textWidth, maxTextHeight,
        maxLines))
    {
        result.text = normalized;
        result.retainedGraphemes = graphemeBoundaries(
            codexMarkdownPlainText(normalized)).size() - 1;
        return result;
    }

    bool separateMarker = maxLines >= 3;
    SearchResult best = findBestCandidate(normalized, boundaries, font,
        textWidth, maxTextHeight, maxLines, separateMarker);
    if (best.text.isEmpty() && separateMarker) {
        best = findBestCandidate(normalized, boundaries, font,
            textWidth, maxTextHeight, maxLines, false);
    }
    if (best.text.isEmpty()) {
        best = findBestPrefix(normalized, boundaries, font, textWidth,
            maxTextHeight, maxLines);
    }
    if (best.text.isEmpty()) {
        best.text = QStringLiteral("…");
    }

    result.text = best.text;
    result.retainedGraphemes = graphemeBoundaries(
        codexMarkdownPlainText(best.text)).size() - 1;
    result.truncated = true;
    return result;
}

int codexBubbleDisplayDurationMs(int visibleGraphemes) {
    int extra = qBound(0,
        (qMax(0, visibleGraphemes) - kDurationGraphemeThreshold)
            * kDurationMsPerGrapheme,
        kMaxDurationMs - kBaseDurationMs);
    return kBaseDurationMs + extra;
}
