// Created by a.kutnyak@rit.va on 2026-07-16.

#include "PageLayout.h"

#include <QDebug>

// PageSequence

PageSequence::PageSequence(const std::size_t firstLineIndex,
                           const std::size_t length,
                           const LineInterval firstLineCrop,
                           const LineInterval lastLineCrop)
    : m_firstLineIndex(firstLineIndex)
    , m_length(length)
    , m_firstLineCrop(firstLineCrop)
    , m_lastLineCrop(lastLineCrop)
{}

// PageSequenceUnwrapper

PageSequenceUnwrapper::PageSequenceUnwrapper(const PageSequence& sequence, const QList<LineLayout>& lines)
    : m_sequence(sequence)
    , m_lines(lines)
{
}

auto PageSequenceUnwrapper::FirstCharIndex() const -> std::size_t
{
    const auto& FirstLine = m_lines[FirstLineIndex()];
    return FirstLine.FirstCharIndex + m_sequence.m_firstLineCrop.First();
}

auto PageSequenceUnwrapper::LastCharIndex() const -> std::size_t
{
    const auto& LastLine = m_lines[LastLineIndex()];
    return LastLine.FirstCharIndex + m_sequence.m_lastLineCrop.Last();
}

auto PageSequenceUnwrapper::FirstLineIndex() const -> std::size_t
{
    return m_sequence.m_firstLineIndex;
}

auto PageSequenceUnwrapper::LastLineIndex() const -> std::size_t
{
    return m_sequence.m_firstLineIndex + m_sequence.m_length - 1;
}

auto PageSequenceUnwrapper::Geometry() const -> QList<QRectF>
{
    QList<QRectF> geometry;

    geometry << m_lines[FirstLineIndex()].GeometryOfUnchecked(m_sequence.m_firstLineCrop);

    if (m_sequence.m_length > 2)
    {
        for (std::size_t i = FirstLineIndex() + 1; i <= LastLineIndex() - 1; ++i)
            geometry << m_lines[i].Geometry;
    }

    if (m_sequence.m_length > 1)
        geometry << m_lines[LastLineIndex()].GeometryOfUnchecked(m_sequence.m_lastLineCrop);

    return geometry;
}

// PageLayout

template<typename T>
auto FirstLineRegionStretch(const LineLayout& line, const bool single, const T& region) -> T
{
    if constexpr(std::is_same_v<T, QRectF>)
    {
        if (!single)
        {
            QRectF stretched(region);
            stretched.setRight(line.Geometry.right());
            return stretched;
        }
    }

    return region;
}

template<typename T>
auto LastLineRegionStretch(const LineLayout& line, const bool single, const T& region) -> T
{
    if constexpr(std::is_same_v<T, QRectF>)
    {
        if (!single)
        {
            QRectF stretched(region);
            stretched.setLeft(line.Geometry.left());
            return stretched;
        }
    }

    return region;
}

template<typename T> // CharRange or QRectF
auto LinesAtHelper(const QList<LineLayout>& Lines,
                   const QList<LineLayout>::const_iterator firstLineIt,
                   const QList<LineLayout>::const_iterator lastLineIt,
                   const T& region) -> std::optional<PageSequence>
{
    assert(firstLineIt != Lines.end());
    assert(lastLineIt != Lines.end());                              // not_null guaranteed

    const auto firstLineCrop = firstLineIt->CharIndicesAt(FirstLineRegionStretch(*firstLineIt, firstLineIt == lastLineIt, region));
    const auto lastLineCrop = lastLineIt->CharIndicesAt(LastLineRegionStretch(*lastLineIt, firstLineIt == lastLineIt, region));

    // in case of `QRectF` there are might be "false positive" lines
    if constexpr(std::is_same_v<T, QRectF>)
    {
        if (!firstLineCrop.has_value() || !lastLineCrop.has_value())
            return std::nullopt;
    }

    return PageSequence {
        0ull + std::distance(Lines.begin(), firstLineIt),
        1ull + std::distance(firstLineIt, lastLineIt),
        *firstLineCrop,
        *lastLineCrop
    };
}

auto PageLayout::LinesAt(const CharRange& range) const -> std::optional<PageSequence>
{
    const auto firstLineIt = findFirstLineWithin(range);

    if (firstLineIt == Lines.end())
        return std::nullopt;

    const auto lastLineIt = findLastLineWithin(range);
    return LinesAtHelper(Lines, firstLineIt, lastLineIt, range);
}

auto PageLayout::LinesAt(const QRectF& region) const -> std::optional<PageSequence>
{
    const auto firstLineIt = findFirstLineCrossedBy(region);

    if (firstLineIt == Lines.end())
        return std::nullopt;

    const auto lastLineIt = findLastLineCrossedBy(region);
    return LinesAtHelper(Lines, firstLineIt, lastLineIt, region);
}

auto PageLayout::LineAt(const QPointF& point) const -> std::optional<PageSequence>
{
    for (auto it = Lines.begin(); it != Lines.end(); ++it)
        if (it->Geometry.contains(point))
        {
            return PageSequence {
                0ull + std::distance(Lines.begin(), it),
                1ull,
                { 0, 0ull + it->Chars.size() },
                { 0, 0ull + it->Chars.size() }
            };
        }

    return std::nullopt;
}

auto PageLayout::WordAt(const QPointF& point) const -> std::optional<PageSequence>
{
    for (auto it = Lines.begin(); it != Lines.end(); ++it)
        if (it->Geometry.contains(point))
        {
            const auto lineCrop = it->WordIndicesAt(point);

            // NOTE: no guarantee that lines is not overlapped (rare case), so iterate without break
            if (!lineCrop.has_value())
                continue;

            return PageSequence {
                0ull + std::distance(Lines.begin(), it),
                1ull,
                *lineCrop,
                *lineCrop
            };
        }

    return std::nullopt;
}

auto PageLayout::GeometryOf(const PageSequence& sequence) const -> QList<QRectF>
{
    return PageSequenceUnwrapper(sequence, Lines).Geometry();
}

auto PageLayout::HasTextAt(const QPointF& point) const -> bool
{
    for (auto it = Lines.begin(); it != Lines.end(); ++it)
        if (it->Geometry.contains(point))
            return true;

    return false;
}

auto PageLayout::GetLinkAt(const QPointF& point) const -> std::optional<DocumentLink>
{
    for (const auto& link : Links)
        for (const auto& rect : link.geometry())
            if (rect.contains(point))
                return link;

    return std::nullopt;
}

// TODO: template helper for deduplication

auto PageLayout::findFirstLineWithin(const CharRange& range) const -> QList<LineLayout>::const_iterator
{
    for (auto it = Lines.begin(); it != Lines.end(); ++it)
    {
        const auto A = it->FirstCharIndex;
        const auto B = it->FirstCharIndex + it->Chars.size() - 1;

        if (A <= range.second && range.first <= B)
            return it;
    }

    return Lines.end();
}

auto PageLayout::findLastLineWithin(const CharRange& range) const -> QList<LineLayout>::const_iterator
{
    for (auto it = Lines.rbegin(); it != Lines.rend(); ++it)
    {
        const auto A = it->FirstCharIndex;
        const auto B = it->FirstCharIndex + it->Chars.size() - 1;

        if (A <= range.second && range.first <= B)
            return std::prev(it.base());
    }

    return Lines.end();
}

auto PageLayout::findFirstLineCrossedBy(const QRectF& region) const -> QList<LineLayout>::const_iterator
{
    for (auto it = Lines.begin(); it != Lines.end(); ++it)
        if (it->Geometry.intersects(region))
            return it;

    return Lines.end();
}

auto PageLayout::findLastLineCrossedBy(const QRectF& region) const -> QList<LineLayout>::const_iterator
{
    for (auto it = Lines.rbegin(); it != Lines.rend(); ++it)
        if (it->Geometry.intersects(region))
            return std::prev(it.base());

    return Lines.end();
}
