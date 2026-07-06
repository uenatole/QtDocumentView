#pragma once

#include <Document/API/DocumentParser.h>

class StandardDocumentParser : public DocumentParser
{
public:
    StandardDocumentParser();
    ~StandardDocumentParser() override;

    auto setLayoutCacheLimit(qreal bytes) const -> void;

    auto setDocument(std::shared_ptr<const Document> document) -> void final;

    auto selection() const -> std::unique_ptr<DocumentSelection> final;

    auto hasText(int page, QPointF point) const -> bool final;
    auto hasLink(int page, QPointF point) const -> bool final;
    auto getLink(int page, QPointF point) const -> std::optional<DocumentLink> final;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
