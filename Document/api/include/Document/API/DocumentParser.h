#pragma once

#include <QRectF>

#include "DocumentLink.h"

struct Document;

struct DocumentSelection
{
    virtual ~DocumentSelection() = default;

    struct Word { QPointF point; };
    struct Line { QPointF point; };
    struct Lines { QRectF region; };
    using Option = std::variant<Word, Line, Lines>;

    virtual auto configure(int page, const Option& option) -> void = 0;

    virtual auto empty() const -> bool = 0;
    virtual auto hash() const -> uint64_t = 0;

    virtual auto text() const -> QString = 0;
    virtual auto geometry() const -> QList<QRectF> = 0;
};

struct DocumentParser
{
    virtual ~DocumentParser() = default;

    virtual auto setDocument(std::shared_ptr<const Document> document) -> void = 0;

    virtual auto selection() const -> std::unique_ptr<DocumentSelection> = 0;

    virtual auto hasText(int page, QPointF point) const -> bool = 0;
    virtual auto hasLink(int page, QPointF point) const -> bool = 0;
    virtual auto getLink(int page, QPointF point) const -> std::optional<DocumentLink> = 0;
};
