#pragma once

#include <Document/API/DocumentParser.h>

class StandardDocumentParser final : public DocumentParser
{
public:
    StandardDocumentParser();
    ~StandardDocumentParser() override;

    auto setLayoutCacheLimit(qreal bytes) const -> void;

    auto setDocument(std::shared_ptr<const Document> document) -> void override;

    auto selection() const -> std::unique_ptr<DocumentSelection> override;

    auto hasText(int page, QPointF point) const -> bool override;
    auto hasLink(int page, QPointF point) const -> bool override;
    auto getLink(int page, QPointF point) const -> std::optional<DocumentLink> override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
