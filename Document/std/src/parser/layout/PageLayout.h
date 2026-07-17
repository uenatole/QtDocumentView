// Created by a.kutnyak@rit.va on 2026-07-16.

#pragma once

#include "LineLayout.h"

#include <optional>
#include <QList>
#include <QRectF>

#include <Document/API/DocumentLink.h> // ...
#include <Document/API/DocumentParser.h> // ...

// TODO: replace with "LineSequence"; reintroduce as PagePart = variant<Word, Line, LineSequence>
struct PageSequence
{
    PageSequence(std::size_t firstLineIndex, std::size_t length,
                 LineInterval firstLineCrop,
                 LineInterval lastLineCrop);

private:
    friend struct PageSequenceUnwrapper;

    std::size_t m_firstLineIndex = -1;
    std::size_t m_length = 0;
    LineInterval m_firstLineCrop = { 0, 0 };
    LineInterval m_lastLineCrop = { 0, 0 };
};

// TODO: Pimpl it! Rethink it?
struct PageSequenceUnwrapper
{
    PageSequenceUnwrapper(const PageSequence& sequence, const QList<LineLayout>& lines);

    [[nodiscard]] auto FirstCharIndex() const -> std::size_t;
    [[nodiscard]] auto LastCharIndex() const -> std::size_t;

    [[nodiscard]] auto FirstLineIndex() const -> std::size_t;
    [[nodiscard]] auto LastLineIndex() const -> std::size_t;

    [[nodiscard]] auto Geometry() const -> QList<QRectF>;

private:
    const PageSequence& m_sequence;
    const QList<LineLayout>& m_lines;
};

// TODO: Pimpl it!
struct PageLayout
{
    [[nodiscard]] auto LinesAt(const CharRange& range) const -> std::optional<PageSequence>;

    [[nodiscard]] auto LinesAt(const QRectF& region) const -> std::optional<PageSequence>;
    [[nodiscard]] auto LineAt(const QPointF& point) const -> std::optional<PageSequence>;
    [[nodiscard]] auto WordAt(const QPointF& point) const -> std::optional<PageSequence>;

    [[nodiscard]] auto GeometryOf(const PageSequence& sequence) const -> QList<QRectF>;

    auto HasTextAt(const QPointF& point) const -> bool;
    auto GetLinkAt(const QPointF& point) const -> std::optional<DocumentLink>;

    QSizeF Geometry;
    QList<LineLayout> Lines;
    QList<DocumentLink> Links; // maybe it shouldn't be here?

private:
    [[nodiscard]] auto findFirstLineWithin(const CharRange&) const -> QList<LineLayout>::const_iterator;
    [[nodiscard]] auto findLastLineWithin(const CharRange&) const -> QList<LineLayout>::const_iterator;

    [[nodiscard]] auto findFirstLineCrossedBy(const QRectF&) const -> QList<LineLayout>::const_iterator;
    [[nodiscard]] auto findLastLineCrossedBy(const QRectF&) const -> QList<LineLayout>::const_iterator;
};
