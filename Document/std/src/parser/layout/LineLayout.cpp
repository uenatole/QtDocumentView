// Created by a.kutnyak@rit.va on 2026-07-16.

#include "LineLayout.h"

// LineInterval

LineInterval::LineInterval(const std::size_t first, const std::size_t length)
    : m_first(first)
    , m_length(length)
{}

auto LineInterval::First() const -> std::size_t { return m_first; }
auto LineInterval::Last() const -> std::size_t { return m_first + m_length - 1; }
auto LineInterval::Length() const -> std::size_t { return m_first; }

// LineLayout

auto LineLayout::CharIndicesAt(const CharRange& range) const -> std::optional<LineInterval>
{
    assert(range.first <= range.second);

    if (range.second < FirstCharIndex)
        return std::nullopt;

    const auto left = std::max(range.first, FirstCharIndex) - FirstCharIndex;
    const auto right = std::min(range.second - FirstCharIndex + 0ull, Chars.size() - 1ull);

    return LineInterval {left, right - left + 1 };
}

auto LineLayout::CharIndicesAt(const QRectF& region) const -> std::optional<LineInterval>
{
    assert(region.isValid());

    if (!Geometry.intersects(region)) // guard
        return std::nullopt;

    if (const auto firstIt = findFirstCharCrossedBy(region); firstIt != Chars.end())
    {
        const auto lastIt = findLastCharCrossedBy(region); // not_null guaranteed (at least lastIt == firstIt)
        return LineInterval {
            0ull + std::distance(Chars.begin(), firstIt),
            1ull + std::distance(firstIt, lastIt)
        };
    }

    return std::nullopt;
}

auto LineLayout::WordIndicesAt(const QPointF& point) const -> std::optional<LineInterval>
{
    if (!Geometry.contains(point))
        return std::nullopt;

    if (const auto it = findWordAt(point); it != Words.end())
    {
        return CharIndicesAt({
            QPointF(it->first, Geometry.top()),
            QPointF(it->second, Geometry.bottom()),
        });
    }

    return std::nullopt;
}

auto LineLayout::GeometryOf(const LineInterval& interval) const -> QRectF
{
    if (interval.Last() >= Chars.size())
        return {};

    return GeometryOfUnchecked(interval);
}

auto LineLayout::GeometryOfUnchecked(const LineInterval& interval) const -> QRectF
{
    assert(interval.Last() < Chars.size());

    const auto left = Chars[static_cast<qlonglong>(interval.First())].left();
    const auto right = Chars[static_cast<qlonglong>(interval.Last())].right();

    return {QPointF(left, Geometry.top()), QPointF(right, Geometry.bottom())};
}

auto LineLayout::centerLineOf(const QRectF& box) -> double
{
    return (box.left() + box.right()) / 2;
}

auto LineLayout::findWordAt(const QPointF& point) const -> QList<std::pair<double, double>>::const_iterator
{
    const auto it = std::lower_bound(Words.begin(), Words.end(), point.x(),
        [](const std::pair<double, double>& word, const double x){
            return word.second < x;
        });

    if (it != Words.end() && (it->first <= point.x() && point.x() <= it->second))
        return it;

    return Words.end();
}

auto LineLayout::findFirstCharCrossedBy(const QRectF& region) const -> QList<QRectF>::const_iterator
{
    const auto it = std::lower_bound(Chars.begin(), Chars.end(), region.left(),
        [](const QRectF& box, const double x){
            return centerLineOf(box) < x;
        });

    if (it != Chars.end() && centerLineOf(*it) <= region.right())
        return it;

    return Chars.end();
}

auto LineLayout::findLastCharCrossedBy(const QRectF& region) const -> QList<QRectF>::const_iterator
{
    auto it = std::upper_bound(Chars.begin(), Chars.end(), region.right(),
        [](const double x, const QRectF& range){
            return x < centerLineOf(range);
        });

    if (it != Chars.begin())
    {
        --it;
        if (centerLineOf(*it) >= region.left())
            return it;
    }

    return Chars.end();
}
