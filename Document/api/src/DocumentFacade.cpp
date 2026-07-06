#include "DocumentFacade.h"

#include "Document.h"
#include "DocumentRenderer.h"
#include "DocumentParser.h"

namespace
{
    struct DummyParser : DocumentParser
    {
        auto setDocument(std::shared_ptr<const Document>) -> void final {}

        auto selection() const -> std::unique_ptr<DocumentSelection> final
        {
            struct DummySelection : DocumentSelection
            {
                auto configure(int, const Option&) -> void final {}
                auto empty() const -> bool final { return true; }
                auto hash() const -> uint64_t final { return 0; }
                auto text() const -> QString final { return {}; }
                auto geometry() const -> QList<QRectF> final { return {}; }
            };

            return std::make_unique<DummySelection>();
        }

        auto hasText(int, QPointF) const -> bool final { return false; }
        auto hasLink(int, QPointF) const -> bool override { return false; }
        auto getLink(int, QPointF) const -> std::optional<DocumentLink> override { return std::nullopt; }
    };

    struct DummyRenderer : DocumentRenderer
    {
        auto setDocument(std::shared_ptr<const Document>) -> void final {}

        auto requestPageRender(int page, qreal scale, DocumentRenderFeedback* feedback) const -> std::optional<QImage> override { return std::nullopt; }
    };
}

DocumentFacade::DocumentFacade()
    : m_parser(std::make_shared<DummyParser>())
    , m_renderer(std::make_shared<DummyRenderer>())
{}

DocumentFacade::~DocumentFacade() = default;

auto DocumentFacade::setDocument(const std::shared_ptr<Document>& document) -> void
{
    m_document = document;

    if (m_parser)
        m_parser->setDocument(document);

    if (m_renderer)
        m_renderer->setDocument(document);
}

auto DocumentFacade::setParser(const std::shared_ptr<DocumentParser>& parser) -> void
{
    m_parser = parser;
    m_parser->setDocument(m_document);
}

auto DocumentFacade::setRenderer(const std::shared_ptr<DocumentRenderer>& renderer) -> void
{
    m_renderer = renderer;
    m_renderer->setDocument(m_document);
}

auto DocumentFacade::setRenderFeedback(DocumentRenderFeedback* feedback) -> void
{
    m_rendererFeedback = feedback;
}

auto DocumentFacade::pageCount() const -> int
{
    return m_document->pageCount();
}

auto DocumentFacade::pageSize(int number) const -> QSizeF
{
    return m_document->pagePointSize(number);
}

auto DocumentFacade::requestImage(int number, qreal scale) const -> std::optional<QImage>
{
    return m_renderer->requestPageRender(number, scale, m_rendererFeedback);
}

auto DocumentFacade::hasLink(int page, QPointF point) const -> bool
{
    return m_parser->hasLink(page, point);
}

auto DocumentFacade::getLink(int page, QPointF point) const -> std::optional<DocumentLink>
{
    return m_parser->getLink(page, point);
}

auto DocumentFacade::hasText(int page, QPointF point) const -> bool
{
    return m_parser->hasText(page, point);
}

auto DocumentFacade::selection() const -> std::unique_ptr<DocumentSelection>
{
    return m_parser->selection();
}
