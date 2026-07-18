// Created by a.kutnyak@rit.va on 2026-07-16.

#include <memory>
#include <optional>

#include "layout/PageLayout.h"

struct LineLayoutParser
{
    explicit LineLayoutParser(const std::back_insert_iterator<QList<LineLayout>> inserter_)
        : inserter(inserter_)
    {}

    auto feed(const std::size_t i, const QChar& chr, const QRectF& box) -> void
    {
        if (chr == '\r') return;        // Only LF "\n" and "\r\n" are valid ("\r" is dropped)
        if (chr == '\n')
        {
            if (!layout) return;        // Do not store empty lines

            feedChar(i, chr, box);
            feedWord(i, chr, box);
            commitLine();
            return;
        }

        if (!layout)
        {
            layout = LineLayout {
                .FirstCharIndex = i,
                .Geometry = box,        // some initial value
            };
        }

        // TODO: support geometry violation checks to reorder characters

        feedChar(i, chr, box);
        feedWord(i, chr, box);
    }

    auto flush()
    {
        const auto i = layout->FirstCharIndex + layout->Chars.size();
        const auto lastChar = layout->Chars.last();

        if (wSpaceSequenceLeft)
            commitWSpaceSequence(i, lastChar.right());

        feedWord(i, ' ', lastChar.translated(lastChar.right(), 0));
    }

private:
    auto commitLine() -> void
    {
        *inserter++ = *layout;
        layout.reset();

        assert(!wordLeft.has_value());
        assert(!wSpaceSequenceLeft.has_value());
    }

    auto commitWSpaceSequence(const std::size_t length, const double WSpaceSeqRight) -> void
    {
        assert(layout.has_value());
        assert(wSpaceSequenceLeft.has_value());

        const double WSpaceSeqLeft = *wSpaceSequenceLeft;
        const double WSpaceSeqWidth = WSpaceSeqRight - WSpaceSeqLeft;
        const double WSpaceAvgWidth = WSpaceSeqWidth / static_cast<double>(length);

        // Fill layout->Chars
        const auto WSpaceHeight = layout->Geometry.height();
        const auto WSpaceTop = layout->Geometry.top();

        std::size_t WSpaceCounter = length;
        auto WSpaceRect = QRectF(
                WSpaceSeqLeft,
                WSpaceTop,
                WSpaceAvgWidth,
                WSpaceHeight
            );

        while (WSpaceCounter--)
        {
            layout->Chars.push_back(WSpaceRect);
            layout->Geometry |= WSpaceRect;

            WSpaceRect.translate(WSpaceAvgWidth, 0);
        }

        wSpaceSequenceLeft.reset();
    }

    auto feedChar(const std::size_t i, const QChar& chr, const QRectF& box) -> void
    {
        if (chr.isSpace() && Q_LIKELY(chr != '\n'))
        {
            if (!wSpaceSequenceLeft)
                wSpaceSequenceLeft = layout->Chars.empty()
                                   ? box.left()
                                   : layout->Chars.back().right();

            return; // All WSpace characters (excepts \n) are skipped and processed later by commitWSpaceSequence(...)
        }

        if (wSpaceSequenceLeft)
            commitWSpaceSequence(i - layout->FirstCharIndex - layout->Chars.size(), box.left());

        layout->Chars.emplace_back(box);
        layout->Geometry |= box;

        // Remove gaps between two sequential characters in line
        if (Q_LIKELY(layout->Chars.size() > 1))
        {
            auto& box_i = *std::prev(layout->Chars.end(), 2);
            auto& box_j = *std::prev(layout->Chars.end(), 1);

            const auto avg = (box_i.right() + box_j.left()) / 2;
            box_i.setRight(avg);
            box_j.setLeft(avg);
        }
    }

    auto feedWord(const std::size_t, const QChar& chr, const QRectF& box) -> void
    {
        if (chr.isSpace() && wordLeft)
        {
            const double wordRight = layout->Chars.back().right();
            layout->Words.emplace_back(*wordLeft, wordRight);
            wordLeft.reset();
        }
        else if (!chr.isSpace() && !wordLeft)
        {
            wordLeft = box.left();
        }
    }

    std::back_insert_iterator<QList<LineLayout>> inserter;

    std::optional<LineLayout> layout;

    std::optional<double> wordLeft;
    std::optional<double> wSpaceSequenceLeft;
};

auto ParsePageLayout(const QString& chars, const QList<QRectF>& boxes) -> std::shared_ptr<PageLayout>
{
    assert(chars.size() == boxes.size());

    auto layout = std::make_shared<PageLayout>();

    LineLayoutParser parser(std::back_insert_iterator(layout->Lines));

    for (int i = 0; i < chars.size(); ++i)
        parser.feed(i, chars[i], boxes[i]);

    parser.flush();

    return layout;
}
