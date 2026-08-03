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
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextBoundaryFinder>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QRect>
#include <QStringList>
#include <QVector>

namespace {

constexpr int kBaseDurationMs = 8000;
constexpr int kMaxDurationMs = 12000;
constexpr int kDurationGraphemeThreshold = 80;
constexpr int kDurationMsPerGrapheme = 25;

bool isLocalCodexLinkDestination(QString const& destination) {
    QString token = destination.trimmed();
    if (token.isEmpty() || token.startsWith(QChar('#'))) {
        return false;
    }

    QString lower = token.toLower();
    if (lower.startsWith(QStringLiteral("file:"))) {
        return true;
    }
    if (token.startsWith(QChar('/')) || token.startsWith(QChar('\\')) ||
        token.startsWith(QStringLiteral("//"))) {
        return true;
    }
    if (token.size() >= 3 && token.at(1) == QChar(':') &&
        (token.at(2) == QChar('/') || token.at(2) == QChar('\\'))) {
        return true;
    }

    // A destination without a URI scheme is a relative Markdown path.  Keep
    // the user-facing label but render it as a compact, non-interactive
    // reference.  Recognised schemes (including http(s), mailto and custom
    // protocol links) remain ordinary labels with their targets removed.
    static QRegularExpression const scheme(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    return !scheme.match(token).hasMatch();
}

QString wrapCodexInlineReference(QString const& label) {
    // Markdown code spans can contain backticks when their delimiter run is
    // longer than any run in the label.  This keeps escaped/malformed labels
    // bounded and avoids producing a second parser-sensitive delimiter.
    int longestRun = 0;
    int currentRun = 0;
    for (QChar const ch : label) {
        if (ch == QChar('`')) {
            ++currentRun;
            longestRun = qMax(longestRun, currentRun);
        }
        else {
            currentRun = 0;
        }
    }
    int delimiterLength = qMax(1, longestRun + 1);
    QString delimiter(delimiterLength, QChar('`'));
    return delimiter + label + delimiter;
}

QString escapeRawHtml(QString const& text);
QString flattenCodexReferenceLabel(QString const& label);

bool markdownFenceAt(QString const& text, int position) {
    bool fenced = false;
    QChar marker;
    int fenceLength = 0;
    int lineStart = 0;
    while (lineStart <= text.size()) {
        int lineEnd = text.indexOf(QChar('\n'), lineStart);
        if (lineEnd < 0) {
            lineEnd = text.size();
        }
        int offset = lineStart;
        while (offset < lineEnd && offset - lineStart < 3 &&
            text.at(offset) == QChar(' ')) {
            ++offset;
        }
        int count = 0;
        QChar lineMarker;
        if (offset < lineEnd &&
            (text.at(offset) == QChar('`') ||
                text.at(offset) == QChar('~')))
        {
            lineMarker = text.at(offset);
            while (offset + count < lineEnd &&
                text.at(offset + count) == lineMarker) {
                ++count;
            }
        }
        if (count >= 3) {
            if (!fenced) {
                fenced = true;
                marker = lineMarker;
                fenceLength = count;
            }
            else if (marker == lineMarker && count >= fenceLength) {
                fenced = false;
                marker = QChar();
                fenceLength = 0;
            }
        }
        if (position <= lineEnd) {
            return fenced;
        }
        if (lineEnd >= text.size()) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return fenced;
}

QString stripMarkdownLinkDestinationsImpl(QString text,
    bool wrapLocalReference) {
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

        // Link-looking text inside a fenced block is source code, not a
        // Markdown link. Preserve the complete line so sanitization does not
        // rewrite URLs or bracket expressions in the code sample.
        if (markdownFenceAt(text, open)) {
            int lineEnd = text.indexOf(QChar('\n'), open);
            if (lineEnd < 0) {
                lineEnd = text.size();
            }
            result += text.mid(cursor, lineEnd - cursor);
            if (lineEnd < text.size()) {
                result += QChar('\n');
                cursor = lineEnd + 1;
            }
            else {
                cursor = text.size();
            }
            continue;
        }

        int labelStart = open + 1;
        bool image = open > 0 && text.at(open - 1) == QChar('!');
        int close = -1;
        int labelDepth = 1;
        bool escaped = false;
        for (int index = labelStart; index < text.size(); ++index) {
            QChar ch = text.at(index);
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == QChar('\\')) {
                escaped = true;
                continue;
            }
            if (ch == QChar('[')) {
                ++labelDepth;
            }
            else if (ch == QChar(']')) {
                --labelDepth;
                if (labelDepth == 0) {
                    close = index;
                    break;
                }
            }
            else if (ch == QChar('\n') || ch == QChar('\r')) {
                break;
            }
        }
        if (close < 0 || close + 1 >= text.size() ||
            text.at(close + 1) != QChar('('))
        {
            // Not an inline link. Copy the opening bracket and keep scanning
            // after it, so ordinary bracketed prose remains unchanged while
            // a valid nested link can still be found on a later pass.
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
                break;
            }
        }

        int destinationEnd = end;
        bool completeDestination = end < text.size() && depth == 0;
        if (!completeDestination) {
            // An incomplete destination is still untrusted. Keep the label,
            // discard the rest of that line's target, and resume after a
            // newline so malformed input cannot leak a path or URL.
            destinationEnd = text.indexOf(QChar('\n'), close + 2);
            if (destinationEnd < 0) {
                destinationEnd = text.size();
            }
        }

        QString destination = text.mid(close + 2,
            destinationEnd - close - 2);
        QString destinationToken = destination.trimmed();
        if (destinationToken.startsWith(QChar('<'))) {
            int closingAngle = destinationToken.indexOf(QChar('>'), 1);
            if (closingAngle > 0) {
                destinationToken = destinationToken.mid(1,
                    closingAngle - 1);
            }
        }
        else {
            int whitespace = -1;
            for (int i = 0; i < destinationToken.size(); ++i) {
                if (destinationToken.at(i).isSpace()) {
                    whitespace = i;
                    break;
                }
            }
            if (whitespace >= 0) {
                destinationToken.truncate(whitespace);
            }
        }

        int prefixEnd = image && open > cursor ? open - 1 : open;
        result += text.mid(cursor, prefixEnd - cursor);
        QString label = stripMarkdownLinkDestinationsImpl(
            text.mid(labelStart, close - labelStart), false);
        if (wrapLocalReference &&
            isLocalCodexLinkDestination(destinationToken)) {
            result += wrapCodexInlineReference(
                flattenCodexReferenceLabel(label));
        }
        else {
            result += label;
        }
        if (completeDestination) {
            cursor = end + 1;
        }
        else {
            if (destinationEnd < text.size()) {
                result += QChar('\n');
                cursor = destinationEnd + 1;
            }
            else {
                cursor = text.size();
            }
        }
    }
    return result;
}

QString stripMarkdownLinkDestinations(QString text) {
    return stripMarkdownLinkDestinationsImpl(text, true);
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

QString flattenCodexReferenceLabel(QString const& label) {
    // A local reference is displayed as one compact code span. Parse the
    // already-sanitized label once to preserve its visible text while
    // discarding nested emphasis/backtick markers and Markdown escapes.
    QTextDocument document;
    document.setUndoRedoEnabled(false);
    document.setMarkdown(escapeRawHtml(label),
        QTextDocument::MarkdownDialectGitHub);
    return document.toPlainText();
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

struct SearchResult {
    QString text;
    int retainedGraphemes = 0;
};

QString closeOpenMarkdown(QString text) {
    text = text.trimmed();
    if (text.isEmpty()) {
        return text;
    }

    // Do not leave an escape marker or an empty list item at the cut point.
    // Both are common when a grapheme budget lands in the middle of a line and
    // otherwise produce a visible, context-free punctuation fragment.
    int trailingBackslashes = 0;
    for (int i = text.size() - 1; i >= 0 &&
        text.at(i) == QChar('\\'); --i) {
        ++trailingBackslashes;
    }
    if ((trailingBackslashes & 1) != 0) {
        text.chop(1);
        text = text.trimmed();
    }
    int lineStart = text.lastIndexOf(QChar('\n')) + 1;
    QString lastLine = text.mid(lineStart).trimmed();
    static QRegularExpression const emptyBlockMarker(
        QStringLiteral("^(?:[-+*]|\\d+[.)]|#{1,6})\\s*$"));
    if (emptyBlockMarker.match(lastLine).hasMatch()) {
        text.chop(text.size() - lineStart);
        text = text.trimmed();
    }

    // A prefix can end after an opening link/image bracket.  Sanitization
    // removes complete destinations before this point, but an incomplete
    // label should not leave a dangling `[` as the last visible context.
    int openBracket = -1;
    bool escaped = false;
    for (int i = 0; i < text.size(); ++i) {
        QChar ch = text.at(i);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == QChar('\\')) {
            escaped = true;
        }
        else if (ch == QChar('[')) {
            openBracket = i;
        }
        else if (ch == QChar(']')) {
            openBracket = -1;
        }
    }
    if (openBracket >= 0) {
        if (openBracket > 0 && text.at(openBracket - 1) == QChar('!')) {
            --openBracket;
        }
        text.truncate(openBracket);
        text = text.trimmed();
        lineStart = text.lastIndexOf(QChar('\n')) + 1;
        lastLine = text.mid(lineStart).trimmed();
        if (emptyBlockMarker.match(lastLine).hasMatch()) {
            text.chop(text.size() - lineStart);
            text = text.trimmed();
        }
    }

    // A previous coarse compaction (or the old head/tail formatter) may have
    // left a standalone ellipsis line immediately before the omitted tail.
    // Drop that generated marker so the new terminal ellipsis is the only
    // truncation cue and no detached punctuation is shown.
    static QRegularExpression const standaloneEllipsis(
        QStringLiteral("^(?:…|\\.\\.\\.)$"));
    QStringList cleanedLines;
    for (QString const& line : text.split(QChar('\n'))) {
        if (!standaloneEllipsis.match(line.trimmed()).hasMatch()) {
            cleanedLines.append(line);
        }
    }
    text = cleanedLines.join(QChar('\n')).trimmed();

    // Close an unfinished fenced code block before placing the terminal
    // ellipsis.  We only treat a fence at a Markdown line start as a block
    // delimiter, so ordinary backticks in prose remain untouched.
    QChar openFence;
    int fenceLength = 0;
    QStringList lines = text.split(QChar('\n'));
    for (QString const& line : lines) {
        int offset = 0;
        while (offset < line.size() && offset < 3 &&
            line.at(offset) == QChar(' ')) {
            ++offset;
        }
        if (offset >= line.size()) {
            continue;
        }
        QChar marker = line.at(offset);
        if (marker != QChar('`') && marker != QChar('~')) {
            continue;
        }
        int count = 0;
        while (offset + count < line.size() &&
            line.at(offset + count) == marker) {
            ++count;
        }
        if (count < 3) {
            continue;
        }
        if (openFence.isNull()) {
            openFence = marker;
            fenceLength = count;
        }
        else if (openFence == marker && count >= fenceLength) {
            openFence = QChar();
            fenceLength = 0;
        }
    }
    if (!openFence.isNull()) {
        text += QChar('\n');
        text += QString(fenceLength, openFence);
    }

    // Inline code spans are paired by their backtick run length.  If the cut
    // is inside a span, append the matching run so the ellipsis is outside the
    // code formatting and remains the final visible character.
    QVector<int> codeRuns;
    bool inFence = false;
    QChar fenceMarker;
    int activeFenceLength = 0;
    for (QString const& line : text.split(QChar('\n'))) {
        int offset = 0;
        while (offset < line.size() && offset < 3 &&
            line.at(offset) == QChar(' ')) {
            ++offset;
        }
        QChar lineFenceMarker;
        int lineFenceLength = 0;
        if (offset < line.size() &&
            (line.at(offset) == QChar('`') ||
                line.at(offset) == QChar('~'))) {
            int count = 0;
            while (offset + count < line.size() &&
                line.at(offset + count) == line.at(offset)) {
                ++count;
            }
            if (count >= 3) {
                lineFenceMarker = line.at(offset);
                lineFenceLength = count;
            }
        }
        if (inFence) {
            if (lineFenceMarker == fenceMarker &&
                lineFenceLength >= activeFenceLength) {
                inFence = false;
            }
            continue;
        }
        if (!lineFenceMarker.isNull()) {
            inFence = true;
            fenceMarker = lineFenceMarker;
            activeFenceLength = lineFenceLength;
            continue;
        }

        for (int i = 0; i < line.size();) {
            if (line.at(i) == QChar('\\')) {
                i += qMin(2, line.size() - i);
                continue;
            }
            if (line.at(i) != QChar('`')) {
                ++i;
                continue;
            }
            int start = i;
            while (i < line.size() && line.at(i) == QChar('`')) {
                ++i;
            }
            int runLength = i - start;
            if (runLength < 3) {
                if (!codeRuns.isEmpty() && codeRuns.constLast() == runLength) {
                    codeRuns.removeLast();
                }
                else {
                    codeRuns.append(runLength);
                }
            }
        }
    }
    for (auto iterator = codeRuns.crbegin(); iterator != codeRuns.crend();
        ++iterator) {
        text += QString(*iterator, QChar('`'));
    }

    // Strong emphasis is the only other delimiter we repair deliberately.
    // Restricting this to paired runs avoids turning ordinary multiplication
    // or identifier underscores into synthetic formatting.
    int strongAsteriskRuns = 0;
    int strongUnderscoreRuns = 0;
    for (int i = 0; i + 1 < text.size();) {
        if (text.at(i) == QChar('\\')) {
            i += qMin(2, text.size() - i);
            continue;
        }
        QChar marker = text.at(i);
        if ((marker != QChar('*') && marker != QChar('_')) ||
            text.at(i + 1) != marker) {
            ++i;
            continue;
        }
        int runLength = 0;
        while (i + runLength < text.size() &&
            text.at(i + runLength) == marker) {
            ++runLength;
        }
        if (runLength == 2) {
            if (marker == QChar('*')) {
                ++strongAsteriskRuns;
            }
            else {
                ++strongUnderscoreRuns;
            }
        }
        i += runLength;
    }
    if ((strongAsteriskRuns & 1) != 0) {
        text += QStringLiteral("**");
    }
    if ((strongUnderscoreRuns & 1) != 0) {
        text += QStringLiteral("__");
    }
    return text.trimmed();
}

QString makePrefixCandidate(QString const& text,
    QVector<int> const& boundaries, int retainedGraphemes)
{
    int totalGraphemes = boundaries.size() - 1;
    if (retainedGraphemes <= 0 || totalGraphemes <= 0) {
        return QStringLiteral("…");
    }
    int count = qMin(retainedGraphemes, totalGraphemes);
    QString prefix = closeOpenMarkdown(text.left(boundaries.at(count)));
    while (prefix.endsWith(QChar(0x2026))) {
        prefix.chop(1);
        prefix = prefix.trimmed();
    }
    if (prefix.isEmpty()) {
        return QStringLiteral("…");
    }

    // A repaired fenced block must end at a Markdown line boundary.  Appending
    // the marker directly after the closing fence (for example `````…) makes
    // that fence invalid on Qt's Unix Markdown parser, which then treats the
    // marker as code and drops it from toPlainText().  Keep the terminal
    // ellipsis visible and outside the code block on every platform.
    static QRegularExpression const fenceOnlyLine(
        QStringLiteral("(?:^|\\n) {0,3}(?:`{3,}|~{3,})[ \\t]*$"));
    if (fenceOnlyLine.match(prefix).hasMatch()) {
        prefix += QChar('\n');
    }
    return prefix + QStringLiteral("…");
}

SearchResult findBestPrefix(QString const& text,
    QVector<int> const& boundaries, QFont const& font,
    int width, int maxHeight, int maxLines)
{
    int totalGraphemes = boundaries.size() - 1;
    int low = 0;
    int high = qMax(1, totalGraphemes - 1);
    SearchResult best;

    while (low <= high) {
        int candidateBudget = low + (high - low) / 2;
        QString candidate = makePrefixCandidate(text, boundaries,
            candidateBudget);
        if (codexMarkdownFits(candidate, font, width, maxHeight, maxLines)) {
            best.text = candidate;
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

void styleCodexInlineCode(QTextDocument &document) {
    QPalette palette;
    if (QGuiApplication::instance() != nullptr) {
        palette = QGuiApplication::palette();
    }
    QColor background = palette.color(QPalette::AlternateBase);
    QColor foreground = palette.color(QPalette::Text);

    // Capture ranges first because merging a format can split fragments while
    // the document is being traversed.  Qt's Markdown parser marks both
    // inline-code spans and fenced code with a fixed-pitch format; using that
    // semantic marker keeps the visual treatment independent of the user's
    // light/dark/high-contrast theme and never relies on link blue.
    struct CodeRange {
        int position = 0;
        int length = 0;
        bool fenced = false;
    };
    QVector<CodeRange> codeRanges;
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        auto blockFormat = block.blockFormat();
        // Qt records fenced blocks in the block format, while inline code
        // spans carry FontFixedPitch on their fragments.  Do not rely on a
        // font-family string alone: that varies by platform and theme.
        bool fenced = blockFormat.hasProperty(QTextFormat::BlockCodeFence) ||
            blockFormat.hasProperty(QTextFormat::BlockCodeLanguage);
        for (QTextBlock::Iterator iterator = block.begin();
            iterator != block.end(); ++iterator)
        {
            QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            QTextCharFormat format = fragment.charFormat();
            if (fenced || format.fontFixedPitch()) {
                codeRanges.append(CodeRange { fragment.position(),
                    fragment.length(), fenced });
            }
        }
    }

    for (auto const& range : codeRanges) {
        QTextCursor cursor(&document);
        cursor.setPosition(range.position);
        cursor.setPosition(range.position + range.length,
            QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        if (range.fenced) {
            auto fixedFont = QFontDatabase::systemFont(
                QFontDatabase::FixedFont);
            format.setFontFixedPitch(true);
            format.setFontFamily(fixedFont.family());
        }
        format.setBackground(background);
        format.setForeground(foreground);
        format.setUnderlineStyle(QTextCharFormat::NoUnderline);
        format.clearProperty(QTextFormat::AnchorHref);
        format.clearProperty(QTextFormat::AnchorName);
        format.setAnchor(false);
        cursor.mergeCharFormat(format);
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
    styleCodexInlineCode(document);
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

    // Completion notifications are prefix-first.  A tail may contain an
    // isolated tool annotation or path that only made sense in the omitted
    // context, so never construct a head/tail candidate here.  The marker is
    // appended after any repaired Markdown delimiters and therefore remains
    // the final visible character.
    SearchResult best = findBestPrefix(normalized, boundaries, font,
        textWidth, maxTextHeight, maxLines);
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
