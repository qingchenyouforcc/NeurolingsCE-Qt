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

#include <QFontMetrics>
#include <QRect>
#include <QTextBoundaryFinder>
#include <QVector>

#include <utility>

namespace {

constexpr int kBaseDurationMs = 8000;
constexpr int kMaxDurationMs = 12000;
constexpr int kDurationGraphemeThreshold = 80;
constexpr int kDurationMsPerGrapheme = 25;

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

bool fits(QString const& text, QFontMetrics const& metrics,
    int width, int maxHeight, int maxLines)
{
    int safeWidth = qMax(1, width);
    int safeLineSpacing = qMax(1, metrics.lineSpacing());
    int safeHeight = qMin(qMax(0, maxHeight),
        safeLineSpacing * qMax(1, maxLines));
    if (safeHeight <= 0) {
        return false;
    }
    QRect rect = metrics.boundingRect(QRect(0, 0, safeWidth, 0),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
    return rect.height() <= safeHeight;
}

struct SearchResult {
    QString text;
    int retainedGraphemes = 0;
};

SearchResult findBestCandidate(QString const& text,
    QVector<int> const& boundaries, QFontMetrics const& metrics,
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
        if (fits(candidate, metrics, width, maxHeight, maxLines)) {
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
    QVector<int> const& boundaries, QFontMetrics const& metrics,
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
        if (fits(candidate, metrics, width, maxHeight, maxLines)) {
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

}

CodexBubbleExcerpt formatCodexBubbleExcerpt(QString const& source,
    QFont const& font, int textWidth, int maxTextHeight, int maxLines)
{
    CodexBubbleExcerpt result;
    QString normalized = normalizeCodexBubbleText(source);
    if (normalized.isEmpty()) {
        return result;
    }

    auto boundaries = graphemeBoundaries(normalized);
    int totalGraphemes = boundaries.size() - 1;
    QFontMetrics metrics(font);
    if (fits(normalized, metrics, textWidth, maxTextHeight, maxLines)) {
        result.text = normalized;
        result.retainedGraphemes = totalGraphemes;
        return result;
    }

    bool separateMarker = maxLines >= 3;
    SearchResult best = findBestCandidate(normalized, boundaries, metrics,
        textWidth, maxTextHeight, maxLines, separateMarker);
    if (best.text.isEmpty() && separateMarker) {
        best = findBestCandidate(normalized, boundaries, metrics,
            textWidth, maxTextHeight, maxLines, false);
    }
    if (best.text.isEmpty()) {
        best = findBestPrefix(normalized, boundaries, metrics,
            textWidth, maxTextHeight, maxLines);
    }
    if (best.text.isEmpty()) {
        best.text = QStringLiteral("…");
    }

    result.text = best.text;
    result.retainedGraphemes = best.retainedGraphemes;
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
