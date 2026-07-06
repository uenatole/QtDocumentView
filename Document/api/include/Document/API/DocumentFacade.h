#pragma once

#include <memory>
#include <QtGui/QImage>

#include "DocumentLink.h"

struct Document;
struct DocumentRenderFeedback;
struct DocumentRenderer;
struct DocumentParser;
struct DocumentSelection;

class DocumentFacade
{
public:
    DocumentFacade();
    ~DocumentFacade();

    auto setDocument(const std::shared_ptr<Document>& document) -> void;

    auto setParser(const std::shared_ptr<DocumentParser>& parser) -> void;
    auto setRenderer(const std::shared_ptr<DocumentRenderer>& renderer) -> void;

    // TODO: refactor this
    auto setRenderFeedback(DocumentRenderFeedback* feedback) -> void;

    auto pageCount() const -> int;
    auto pageSize(int number) const -> QSizeF;

    auto requestImage(int number, qreal scale) const -> std::optional<QImage>;

    auto selection() const -> std::unique_ptr<DocumentSelection>;

    auto hasText(int page, QPointF point) const -> bool;
    auto hasLink(int page, QPointF point) const -> bool;
    auto getLink(int page, QPointF point) const -> std::optional<DocumentLink>;

private:
    std::shared_ptr<Document> m_document;

    std::shared_ptr<DocumentParser> m_parser;
    std::shared_ptr<DocumentRenderer> m_renderer;
    DocumentRenderFeedback* m_rendererFeedback = nullptr;
};
