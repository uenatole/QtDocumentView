#include "StandardDocumentParser.h"

#include <optional>

#include <QLoggingCategory>

#include <Document/API/Document.h>

#include "layout/DocumentLayout.h"
#include "layout/PageLayout.h"

// Defined at "StandardPageParsing.cpp"
auto ParsePageLayout(const QString& chars, const QList<QRectF>& boxes) -> std::shared_ptr<PageLayout>;

Q_LOGGING_CATEGORY(PAGE_LAYOUT_DUMP_DCAT, "std.parser.page_layout_dump")

struct StandardDocumentParser::Private
{
    auto SetMemoryLimit(std::size_t bytes) -> void
    {
        documentLayout.SetMemoryLimit(bytes);
    }

    auto SetDocument(const std::shared_ptr<const Document>& doc) -> void
    {
        document = doc;
        documentLayout.Reset(); // TODO: also terminate all active work
    }

    using AmortizedIndex = std::pair<PageSequence, CharRange>;

    auto LinesAt(const int page, const QRectF& rect) const -> std::optional<AmortizedIndex>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        const auto pageSequenceOpt = pageLayoutPtr->LinesAt(rect);
        return getAmortizedIndex(*pageLayoutPtr, pageSequenceOpt);
    }

    auto LinesAt(const int page, const CharRange& range) const -> std::optional<AmortizedIndex>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        const auto pageSequenceOpt = pageLayoutPtr->LinesAt(range);
        return getAmortizedIndex(*pageLayoutPtr, pageSequenceOpt);
    }

    auto LineAt(const int page, const QPointF& point) const -> std::optional<AmortizedIndex>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        const auto pageSequenceOpt = pageLayoutPtr->LineAt(point);
        return getAmortizedIndex(*pageLayoutPtr, pageSequenceOpt);
    }

    auto WordAt(const int page, const QPointF& point) const -> std::optional<AmortizedIndex>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        const auto pageSequenceOpt = pageLayoutPtr->WordAt(point);
        return getAmortizedIndex(*pageLayoutPtr, pageSequenceOpt);
    }

    auto GetText(const int page, const CharRange& range) const -> QString
    {
        return document->text(page, range.first, range.second - range.first + 1);
    }

    auto GetGeometry(const int page, const PageSequence& sequence) const -> QList<QRectF>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        return pageLayoutPtr->GeometryOf(sequence);
    }

    auto HasTextAt(const int page, const QPointF& point) const -> bool
    {
        const auto pageLayoutPtr = getPageLayout(page);
        return pageLayoutPtr->HasTextAt(point);
    }

    auto GetLinkAt(const int page, const QPointF& point) const -> std::optional<DocumentLink>
    {
        const auto pageLayoutPtr = getPageLayout(page);
        return pageLayoutPtr->GetLinkAt(point);
    }

private:
    static auto getAmortizedIndex(const PageLayout& pageLayout, const std::optional<PageSequence>& pageSequenceOpt)
        -> std::optional<AmortizedIndex>
    {
        if (!pageSequenceOpt)
            return std::nullopt;

        const PageSequenceUnwrapper unwrapper(*pageSequenceOpt, pageLayout.Lines);

        return std::pair {
            *pageSequenceOpt,
            CharRange { unwrapper.FirstCharIndex(), unwrapper.LastCharIndex() },
        };
    }

    auto getPageLayout(const int page) const -> std::shared_ptr<const PageLayout>
    {
        auto layout = documentLayout.GetPage(page);

        if (!layout)
        {
            // NOTE: this is because of immediate mode (lack of async API):
            //       if page is not parsed yet, it will be threatened as empty.
            // TODO: remove after async is implemented.
            if (!document->textReady(page))
            {
                (void) document->forceTextReadiness(page);

                static auto EMPTY_PAGE_LAYOUT = std::make_shared<PageLayout>();
                return EMPTY_PAGE_LAYOUT;
            }

            layout = createPageLayout(page);
            documentLayout.InsertPage(page, layout);
        }

        return layout;
    }

    auto createPageLayout(const int page) const -> std::shared_ptr<const PageLayout>
    {
        const auto chars = document->text(page);
        const auto boxes = document->textBoxes(page);
        const auto links = document->links(page);

        const auto pageLayout = ParsePageLayout(chars, boxes);
        pageLayout->Links = links;

        // dumpPageLayout(*pageLayout, page, *document); // NOTE: debug

        return pageLayout;
    }

    static auto dumpPageLayout(const PageLayout& layout,
                               const int page,
                               const Document& document) -> void
    {
        for (const auto &[index, geometry, chars, words] : layout.Lines) {
            qCDebug(PAGE_LAYOUT_DUMP_DCAT) << "    geom: " << geometry;
            qCDebug(PAGE_LAYOUT_DUMP_DCAT) << "    text: " << document.text(page, index, chars.size());
            qCDebug(PAGE_LAYOUT_DUMP_DCAT) << "    chars:" << chars;
            qCDebug(PAGE_LAYOUT_DUMP_DCAT) << "    words:" << words;
        }
    }

    std::shared_ptr<const Document> document;
    mutable DocumentLayout documentLayout;
};

StandardDocumentParser::StandardDocumentParser()
    : d(std::make_unique<Private>())
{}

StandardDocumentParser::~StandardDocumentParser() = default;

auto StandardDocumentParser::setLayoutCacheLimit(const qreal bytes) const -> void
{
    d->SetMemoryLimit(bytes);
}

auto StandardDocumentParser::setDocument(const std::shared_ptr<const Document> document) -> void
{
    d->SetDocument(document);
}

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

auto StandardDocumentParser::selection() const -> std::unique_ptr<DocumentSelection>
{
    struct TextSelection : DocumentSelection
    {
        explicit TextSelection(Private* const d)
            : d_ptr(d)
        {}

        auto configure(const int page, const CharRange range) -> void final
        {
            m_page = page;
            m_index = d_ptr->LinesAt(page, range);
        }

        auto configure(const int page, const Option& option) -> void final
        {
          std::visit(overload {
                [&](const Lines& lines) {
                    m_page = page;
                    m_index = d_ptr->LinesAt(page, lines.region);
                },
                [&](const Line& line) {
                    m_page = page;
                    m_index = d_ptr->LineAt(page, line.point);
                },
                [&](const Word& word) {
                    m_page = page;
                    m_index = d_ptr->WordAt(page, word.point);
                },
                [&](const None&) {
                    m_page = page;
                    m_index.reset();
                }
            }, option);
        }

        [[nodiscard]] auto range() const -> CharRange final
        {
            return m_index ? m_index->second : std::pair { -1ul, -1ul };
        }

        [[nodiscard]] auto text() const -> QString final
        {
            return m_index ? d_ptr->GetText(m_page, m_index->second) : "";
        }

        [[nodiscard]] auto geometry() const -> QList<QRectF> final
        {
            return m_index ? d_ptr->GetGeometry(m_page, m_index->first) : QList<QRectF>();
        }

    private:
        Private* const d_ptr;

        int m_page = -1;
        std::optional<Private::AmortizedIndex> m_index;
    };

    return std::make_unique<TextSelection>(d.get());
}

auto StandardDocumentParser::hasText(const int page, const QPointF point) const -> bool
{
    return d->HasTextAt(page, point);
}

auto StandardDocumentParser::hasLink(const int page, const QPointF point) const -> bool
{
    return d->GetLinkAt(page, point).has_value();
}

auto StandardDocumentParser::getLink(const int page, const QPointF point) const -> std::optional<DocumentLink>
{
    return d->GetLinkAt(page, point);
}
