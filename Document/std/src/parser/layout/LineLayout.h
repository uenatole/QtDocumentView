// Created by a.kutnyak@rit.va on 2026-07-16.

#pragma once

#include <optional>

#include <Document/API/DocumentParser.h>

// TODO: FastPimpl it!
struct LineInterval
{
    // TODO: introduce 2 constructors: [left; left + length), [left; right] using "tagged types".
    LineInterval(std::size_t first, std::size_t length);

    [[nodiscard]] auto First() const -> std::size_t;
    [[nodiscard]] auto Last() const -> std::size_t;
    [[nodiscard]] auto Length() const -> std::size_t;

private:
    std::size_t m_first;
    std::size_t m_length;
};

// TODO: FastPimpl it!
struct LineLayout
{
    // NOTE: looks like redundant.
    // TODO: consider to removal
    [[nodiscard]] auto CharIndicesAt(const CharRange& range) const -> std::optional<LineInterval>;

    [[nodiscard]] auto CharIndicesAt(const QRectF& region) const -> std::optional<LineInterval>;
    [[nodiscard]] auto WordIndicesAt(const QPointF& point) const -> std::optional<LineInterval>;

    [[nodiscard]] auto GeometryOf(const LineInterval& interval) const -> QRectF;
    [[nodiscard]] auto GeometryOfUnchecked(const LineInterval& interval) const -> QRectF;

    std::size_t FirstCharIndex;
    QRectF Geometry;
    QList<QRectF> Chars;
    QList<std::pair<double, double>> Words;

private:
    static auto centerLineOf(const QRectF& box) -> double;

    [[nodiscard]] auto findWordAt(const QPointF& point) const -> QList<std::pair<double, double>>::const_iterator;

    [[nodiscard]] auto findFirstCharCrossedBy(const QRectF& region) const -> QList<QRectF>::const_iterator;
    [[nodiscard]] auto findLastCharCrossedBy(const QRectF& region) const -> QList<QRectF>::const_iterator;
};
