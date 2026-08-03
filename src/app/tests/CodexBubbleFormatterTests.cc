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

#include "../ui/widgets/CodexBubbleFormatter.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QRect>
#include <QString>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>

#include <cstdlib>
#include <iostream>

namespace {

int g_failures = 0;

void expect(bool condition, char const *message) {
    if (condition) {
        return;
    }
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

QRect measuredRect(QString const& text, QFont const& font, int width) {
    QFontMetrics metrics(font);
    return metrics.boundingRect(QRect(0, 0, width, 0),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
}

void testShortText(QFont const& font) {
    auto result = formatCodexBubbleExcerpt(QStringLiteral("short"), font,
        320, 200, 8);
    expect(result.text == QStringLiteral("short") && !result.truncated,
        "short Codex text should remain unchanged");
    expect(result.retainedGraphemes == 5,
        "short Codex text should report visible grapheme count");
}

void testLongText(QFont const& font) {
    QString text = QStringLiteral("BEGIN\n")
        + QString(1200, QChar(0x4e2d))
        + QStringLiteral("\nFINAL\nobject\\NeurolingsCE }\n"
            "::git-push{... branch=\"main\"}");
    QFontMetrics metrics(font);
    int maxHeight = 8 * metrics.lineSpacing();
    auto result = formatCodexBubbleExcerpt(text, font, 320, maxHeight, 8);
    expect(result.truncated, "long Codex text should be marked truncated");
    QString plain = codexMarkdownPlainText(result.text).trimmed();
    expect(result.text.contains(QStringLiteral("BEGIN")) &&
        plain.endsWith(QStringLiteral("…")) &&
        !plain.contains(QStringLiteral("FINAL")) &&
        !plain.contains(QStringLiteral("object\\NeurolingsCE")) &&
        !plain.contains(QStringLiteral("::git-push")),
        "long Codex text should retain only the completion prefix");
    expect(measuredRect(result.text, font, 320).height() <= maxHeight,
        "formatted Codex text should fit its height budget");
}

void testEmojiDoesNotLoop(QFont const& font) {
    QString text = QStringLiteral("BEGIN ");
    text += QStringLiteral("😀").repeated(4096);
    text += QStringLiteral(" FINAL");
    QFontMetrics metrics(font);
    auto result = formatCodexBubbleExcerpt(text, font, 320,
        8 * metrics.lineSpacing(), 8);
    expect(!result.text.isEmpty() && result.truncated,
        "emoji-heavy Codex text should return a bounded excerpt");
    expect(result.text.contains(QStringLiteral("BEGIN")) &&
        codexMarkdownPlainText(result.text).trimmed().endsWith(
            QStringLiteral("…")),
        "emoji-heavy Codex text should preserve a complete prefix and ellipsis");
}

void testMultilineAndLargeFont(QFont font) {
    QString text = QStringLiteral("# Heading\n\n- item one\n- item two\n```\ncode\n```\n")
        + QString(800, QLatin1Char('x'));
    QFontMetrics metrics(font);
    auto result = formatCodexBubbleExcerpt(text, font, 320,
        8 * metrics.lineSpacing(), 8);
    expect(!result.text.isEmpty(), "multiline Codex text should not disappear");
    font.setPointSizeF(font.pointSizeF() * 4.0);
    QFontMetrics largeMetrics(font);
    auto large = formatCodexBubbleExcerpt(text, font, 320,
        4 * largeMetrics.lineSpacing(), 8);
    expect(!large.text.isEmpty() && large.text.contains(QStringLiteral("…")),
        "large-font Codex text should use an ellipsis fallback");
}

void testDurations() {
    expect(codexBubbleDisplayDurationMs(0) == 8000,
        "empty Codex excerpts should use the base duration");
    expect(codexBubbleDisplayDurationMs(40) == 8000,
        "short Codex excerpts should use the base duration");
    expect(codexBubbleDisplayDurationMs(80) == 8000,
        "80 graphemes should use the base duration");
    expect(codexBubbleDisplayDurationMs(160) == 10000,
        "160 graphemes should use a ten-second duration");
    expect(codexBubbleDisplayDurationMs(240) == 12000 &&
        codexBubbleDisplayDurationMs(4096) == 12000,
        "long Codex excerpts should cap at twelve seconds");
}

void testMarkdownRenderingAndSafety(QFont const& font) {
    QString source = QStringLiteral("# **完成**\n\n"
        "- `命令`\n- _结果_\n\n"
        "[安全标签](https://example.invalid/never-open)\n"
        "<script>alert('x')</script>");
    QString sanitized = sanitizeCodexMarkdown(source);
    expect(!sanitized.contains(QStringLiteral("example.invalid")),
        "Codex Markdown sanitization should remove link destinations");
    expect(sanitized.contains(QStringLiteral("**完成**")) &&
        sanitized.contains(QStringLiteral("`命令`")),
        "Codex Markdown sanitization should preserve safe formatting markers");
    expect(sanitized.contains(QStringLiteral("&lt;script&gt;")),
        "Codex Markdown sanitization should escape raw HTML tags");

    auto excerpt = formatCodexBubbleExcerpt(source, font, 320, 200, 8);
    expect(!excerpt.truncated && excerpt.text.contains(QStringLiteral("**完成**")),
        "short Codex Markdown should retain formatting source");

    QTextDocument document;
    configureCodexMarkdownDocument(document, excerpt.text, font, 320);
    expect(document.toPlainText().contains(QStringLiteral("完成")) &&
        document.toPlainText().contains(QStringLiteral("命令")),
        "configured Codex document should expose Markdown content as text");
    expect(!document.toHtml().contains(QStringLiteral("<script"),
        Qt::CaseInsensitive),
        "configured Codex document should not contain executable HTML tags");
    bool sawBold = false;
    bool sawAnchor = false;
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        for (QTextBlock::Iterator iterator = block.begin();
            iterator != block.end(); ++iterator)
        {
            auto fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            auto format = fragment.charFormat();
            sawBold = sawBold || format.fontWeight() >= QFont::Bold;
            sawAnchor = sawAnchor || format.isAnchor() ||
                format.hasProperty(QTextFormat::AnchorHref);
        }
    }
    expect(sawBold, "Codex Markdown document should render bold text");
    expect(!sawAnchor,
        "Codex Markdown document should clear links and remain non-interactive");
    expect(codexMarkdownFits(source, font, 320, 200, 8),
        "formatted Codex Markdown should use document layout for fit checks");
}

void testFencedCodeBlocks(QFont const& font) {
    QString source = QStringLiteral(
        "前置说明\n\n```cpp\n"
        "const QString message = QStringLiteral(\"你好\");\n"
        "const char *url = \"https://example.invalid/[x]\";\n"
        "qDebug() << message;\n"
        "```\n\n后置说明");
    auto sanitized = sanitizeCodexMarkdown(source);
    expect(sanitized.contains(QStringLiteral("```cpp")) &&
        sanitized.contains(QStringLiteral("qDebug()")),
        "Codex Markdown sanitization should preserve fenced code blocks");

    QTextDocument document;
    configureCodexMarkdownDocument(document, source, font, 320);
    expect(document.toPlainText().contains(QStringLiteral("const QString message")) &&
        document.toPlainText().contains(QStringLiteral("qDebug()")) &&
        document.toPlainText().contains(QStringLiteral("https://example.invalid/[x]")),
        "fenced Codex code should remain visible in the document");
    bool sawFixedPitch = false;
    bool sawCodeBackground = false;
    QColor expectedBackground = QGuiApplication::palette().color(
        QPalette::AlternateBase);
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        for (QTextBlock::Iterator iterator = block.begin();
            iterator != block.end(); ++iterator)
        {
            auto fragment = iterator.fragment();
            if (!fragment.isValid() ||
                !fragment.text().contains(QStringLiteral("const QString"))) {
                continue;
            }
            auto format = fragment.charFormat();
            sawFixedPitch = format.fontFixedPitch();
            sawCodeBackground = format.background().style() != Qt::NoBrush &&
                format.background().color() == expectedBackground;
        }
    }
    expect(sawFixedPitch,
        "fenced Codex code should use a fixed-pitch text format");
    expect(sawCodeBackground,
        "fenced Codex code should use the palette-aware code background");
    expect(codexMarkdownFits(source, font, 320, 220, 8),
        "a short fenced Codex block should fit the bubble budget");
}

void testCodexFileReferenceRendering(QFont const& font) {
    QString source = QStringLiteral(
        "这是一个本地引用：[项目说明 README.md](D:/repo/README.md)\n"
        "相对路径：[源码](docs/src/main.cc)\n"
        "file URL：[日志](file:///D:/repo/log.txt)\n"
        "强调标签：[**README.md**](D:/repo/README.md)\n"
        "外部链接：[文档](https://example.invalid/never-open)\n"
        "图片语法：![预览](D:/repo/preview.png)");
    QString sanitized = sanitizeCodexMarkdown(source);
    expect(sanitized.contains(QStringLiteral("`项目说明 README.md`")) &&
        sanitized.contains(QStringLiteral("`源码`")) &&
        sanitized.contains(QStringLiteral("`日志`")) &&
        sanitized.contains(QStringLiteral("`README.md`")) &&
        sanitized.contains(QStringLiteral("`预览`")),
        "local and relative Codex links should become inline references");
    expect(sanitized.contains(QStringLiteral("文档")) &&
        !sanitized.contains(QStringLiteral("https://example.invalid")) &&
        !sanitized.contains(QStringLiteral("D:/repo")) &&
        !sanitized.contains(QStringLiteral("file:///")),
        "link destinations must not leak into sanitized Codex text");
    expect(sanitizeCodexMarkdown(sanitized) == sanitized,
        "Codex link sanitization should remain idempotent");

    QTextDocument document;
    configureCodexMarkdownDocument(document, source, font, 320);
    expect(document.toPlainText().contains(QStringLiteral("项目说明 README.md")) &&
        document.toPlainText().contains(QStringLiteral("文档")) &&
        !document.toPlainText().contains(QStringLiteral("D:/repo")) &&
        !document.toPlainText().contains(QStringLiteral("https://example.invalid")),
        "configured Codex document should show labels but never destinations");

    bool sawReferenceCode = false;
    bool sawReferenceBackground = false;
    bool sawAnchor = false;
    QColor expectedBackground = QGuiApplication::palette().color(
        QPalette::AlternateBase);
    for (QTextBlock block = document.begin(); block != document.end();
        block = block.next())
    {
        for (QTextBlock::Iterator iterator = block.begin();
            iterator != block.end(); ++iterator)
        {
            auto fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            auto format = fragment.charFormat();
            sawAnchor = sawAnchor || format.isAnchor() ||
                format.hasProperty(QTextFormat::AnchorHref) ||
                format.hasProperty(QTextFormat::AnchorName);
            if (fragment.text().contains(QStringLiteral("项目说明"))) {
                sawReferenceCode = format.fontFixedPitch();
                sawReferenceBackground = format.background().style() !=
                    Qt::NoBrush && format.background().color() ==
                    expectedBackground;
            }
        }
    }
    expect(sawReferenceCode,
        "local file labels should use a compact fixed-pitch reference style");
    expect(sawReferenceBackground,
        "reference style should use a palette-aware background");
    expect(!sawAnchor,
        "local and external Codex references must remain non-interactive");
}

void testMalformedAndNestedLinksStaySafe() {
    QString malformed = QStringLiteral(
        "[坏的本地标签](D:/secret/project\n"
        "[外层 [内层](https://secret.invalid)](D:/secret/outer)\n"
        "[转义\\]标签](../relative/file.txt)");
    QString sanitized = sanitizeCodexMarkdown(malformed);
    expect(!sanitized.contains(QStringLiteral("D:/secret")) &&
        !sanitized.contains(QStringLiteral("https://secret.invalid")),
        "malformed and nested link targets must be removed");
    expect(sanitized.contains(QStringLiteral("坏的本地标签")) &&
        sanitized.contains(QStringLiteral("外层 内层")) &&
        sanitized.contains(QStringLiteral("`转义]标签`")),
        "malformed and nested links should retain readable labels");
}

void testReferencePrefixTruncation(QFont const& font) {
    QString source = QStringLiteral("完成摘要：") +
        QString(140, QLatin1Char('x')) +
        QStringLiteral(" [README.md](D:/repo/README.md:12) ") +
        QString(600, QLatin1Char('y'));
    QFontMetrics metrics(font);
    int maxHeight = 4 * metrics.lineSpacing();
    auto result = formatCodexBubbleExcerpt(source, font, 320, maxHeight, 4);
    QString plain = codexMarkdownPlainText(result.text).trimmed();
    expect(result.truncated && plain.endsWith(QStringLiteral("…")),
        "prefix truncation near a local reference should keep a terminal ellipsis");
    expect(!result.text.contains(QStringLiteral("D:/repo")) &&
        codexMarkdownFits(result.text, font, 320, maxHeight, 4),
        "truncated local references should remain safe and fit the bubble");
}

void testMarkdownPrefixClosesStructure(QFont const& font) {
    QString source = QStringLiteral("**完成摘要**\n\n"
        "- 已更新桌宠通知\n"
        "- 已运行 `NeurolingsCEBubbleTests`\n"
        "…\n"
        "```text\n"
        "object\\NeurolingsCE }\n"
        "::git-push{... branch=\"main\"}\n"
        "```");
    QFontMetrics metrics(font);
    auto result = formatCodexBubbleExcerpt(source, font, 320,
        4 * metrics.lineSpacing(), 4);
    QString plain = codexMarkdownPlainText(result.text).trimmed();
    expect(result.truncated, "long Markdown notification should be truncated");
    expect(plain.startsWith(QStringLiteral("完成摘要")) &&
        plain.endsWith(QStringLiteral("…")),
        "Markdown prefix truncation should keep the summary and terminal ellipsis");
    expect(!plain.contains(QStringLiteral("\n…\n")),
        "Markdown prefix truncation should remove detached ellipsis lines");
    expect(!plain.contains(QStringLiteral("::git-push")) &&
        !plain.contains(QStringLiteral("object\\NeurolingsCE")),
        "Markdown prefix truncation should drop tool/path tail fragments");
    expect(codexMarkdownFits(result.text, font, 320,
        4 * metrics.lineSpacing(), 4),
        "Markdown prefix excerpt should fit the line and height budget");
}

void testFenceClosureKeepsEllipsisOnOwnLine(QFont const& font) {
    // Keep the code lines short and the width generous so this checks the
    // Markdown fence boundary rather than a platform's glyph widths. The
    // prefix budget must cut inside the block and synthesize its closing
    // fence before appending the truncation marker.
    QString source = QStringLiteral("```text\n")
        + QStringLiteral("line\n").repeated(40)
        + QStringLiteral("```");
    QFontMetrics metrics(font);
    int maxHeight = 6 * metrics.lineSpacing();
    auto result = formatCodexBubbleExcerpt(source, font, 2000, maxHeight, 6);
    expect(result.truncated,
        "long fenced Markdown should produce a bounded excerpt");
    expect(result.text.contains(QStringLiteral("```\n…")),
        "the fenced-code closing delimiter and ellipsis should be separate lines");
    expect(codexMarkdownPlainText(result.text).trimmed().endsWith(
        QStringLiteral("…")),
        "the fence-closed excerpt should retain a visible terminal ellipsis");
}

}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    QFont font = QGuiApplication::font();
    if (font.pointSizeF() <= 0) {
        font.setPixelSize(13);
    }

    testShortText(font);
    testLongText(font);
    testEmojiDoesNotLoop(font);
    testMultilineAndLargeFont(font);
    testDurations();
    testMarkdownRenderingAndSafety(font);
    testFencedCodeBlocks(font);
    testCodexFileReferenceRendering(font);
    testMalformedAndNestedLinksStaySafe();
    testReferencePrefixTruncation(font);
    testMarkdownPrefixClosesStructure(font);
    testFenceClosureKeepsEllipsisOnOwnLine(font);

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "All Codex bubble formatter tests passed" << std::endl;
    return EXIT_SUCCESS;
}
