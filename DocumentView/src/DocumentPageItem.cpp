#include "DocumentPageItem.h"

#include "Document/API/DocumentParser.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>

#include <Document/API/DocumentFacade.h>
#include <Document/API/DocumentParser.h>

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

struct PageTextSelector
{
    using SelectionList = std::vector<std::unique_ptr<DocumentSelection>>;

    explicit PageTextSelector(const int page, const std::shared_ptr<const DocumentFacade>& document)
        : m_page(page)
        , m_document(document)
    {}

    auto empty() const -> bool
    {
        return m_selections.empty();
    }

    auto text() const -> QString
    {
        QString text;
        for (const auto& s : m_selections)
            text += s->text();

        return text;
    }

    auto geometry() const -> QList<QRectF>
    {
        QList<QRectF> geometry;
        for (const auto& s : m_selections)
            geometry += s->geometry();

        return geometry;
    }

    auto resetSelection()
    {
        m_selections.clear();
        m_lastSelectionPos = -1;
    }

    auto updateLastSelection(const DocumentSelection::Option& option) -> void
    {
        if (m_selections.empty() || m_lastSelectionPos == -1)
            return;

        m_selections.erase(m_selections.begin() + m_lastSelectionPos);
        m_lastSelectionPos = -1;

        if (std::holds_alternative<DocumentSelection::None>(option))
            return;

        appendSelection(option);
    }

    auto appendSelection(const DocumentSelection::Option& option) -> void
    {
        assert(!std::holds_alternative<DocumentSelection::None>(option));

        auto newSelection = m_document->selection();
        newSelection->configure(m_page, option);

        mergeSelectionInto(std::move(newSelection));
    }

// NOTE: Current implementation is inefficient due to the large amount of
//       order-based conditional code inside the loop over the ordered data.
//       There is also some memory copy/allocation overhead.
//       But it “works TM”, that’s why it’s here.
//
// TODO: Optimize it!
auto mergeSelectionInto(std::unique_ptr<DocumentSelection>&& new_selection) -> void
{
    const auto [a, b] = new_selection->range();

    SelectionList result;
    result.reserve(m_selections.size() + 2);

    bool inserted = false;
    std::optional<std::size_t> leftTouchOpt;
    std::optional<std::size_t> rightTouchOpt;

    for (auto it = m_selections.begin(); it != m_selections.end(); ++it)
    {
        const auto [l, r] = (*it)->range();

        if (r < a)
        {
            result.push_back(std::move(*it));
            continue;
        }

        if (l > b)
        {
            if (!inserted)
            {
                const auto finalLeft = leftTouchOpt.value_or(a);
                const auto finalRight = rightTouchOpt.value_or(b);

                auto merged = m_document->selection();
                merged->configure(m_page, {finalLeft, finalRight});
                result.push_back(std::move(merged));
                m_lastSelectionPos = result.size() - 1;
                inserted = true;
            }
            result.push_back(std::move(*it));
            continue;
        }

        if (l < a)
        {
            if (l <= a - 1)
            {
                auto leftPart = m_document->selection();
                leftPart->configure(m_page, {l, a - 1});
                result.push_back(std::move(leftPart));
            }
        }
        else if (l == a)
        {
            leftTouchOpt = l;
        }

        if (r > b)
        {
            if (b + 1 <= r)
            {
                auto rightPart = m_document->selection();
                rightPart->configure(m_page, {b + 1, r});
                result.push_back(std::move(rightPart));
            }
        }
        else if (r == b)
        {
            rightTouchOpt = r;
        }
    }

    if (!inserted)
    {
        const auto finalLeft = leftTouchOpt.value_or(a);
        const auto finalRight = rightTouchOpt.value_or(b);

        auto merged = m_document->selection();
        merged->configure(m_page, {finalLeft, finalRight});
        result.push_back(std::move(merged));
        m_lastSelectionPos = result.size() - 1;
    }

    m_selections = std::move(result);
}

private:
    const int m_page;
    const std::shared_ptr<const DocumentFacade> m_document;

    SelectionList m_selections;
    std::size_t m_lastSelectionPos = -1;
};

struct DocumentPageItem::Private
{
    friend class DocumentPageItem;

    Private(const std::shared_ptr<DocumentFacade>& document, Feedback* feedback, const int number)
        : document(document)
        , feedback(feedback)
        , number(number)
        , pointSize(document->pageSize(number))
        , textSelector(number, document)
    {}

private:
    std::shared_ptr<DocumentFacade> const document;
    Feedback* const feedback;

    const int number;
    const QSizeF pointSize;

    std::optional<DocumentLink> hoveredLinkOpt;
    PageTextSelector textSelector;
};

DocumentPageItem::DocumentPageItem(const std::shared_ptr<DocumentFacade>& document, Feedback* feedback, const int number)
    : d_ptr(new Private(document, feedback, number))
{
    setCacheMode(NoCache);
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable, true);
    assert(number >= 0 && number < document->pageCount());
}

DocumentPageItem::~DocumentPageItem() = default;

QRectF DocumentPageItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), d_ptr->pointSize);
}

void DocumentPageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);

    const qreal scale = painter->worldTransform().m11();

    // TODO: draw as underlay after other operations to exclude possible composition interference (~~~)
    painter->fillRect(boundingRect(), Qt::white);

    if (const auto image = d_ptr->document->requestImage(d_ptr->number, scale); image)
        painter->drawImage(boundingRect(), *image);

    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Multiply);

#ifdef DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP
    {
        const auto dSelection = d_ptr->document->selection();
        dSelection->configure(d_ptr->number, DocumentSelection::Lines { QRectF(QPointF(0, 0), d_ptr->pointSize) });
        for (const QList<QRectF> geometries = dSelection->geometry(); const QRectF& geometry : geometries)
            painter->drawRect(geometry.adjusted(-0, -2, +0, +2));
    }
#endif

    if (!d_ptr->textSelector.empty())
    {
        // TODO: make text selection style configurable

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(206, 235, 249, 200));

        for (const QList<QRectF> geometries = d_ptr->textSelector.geometry(); const QRectF& geometry : geometries)
            painter->drawRect(geometry.adjusted(-0, -2, +0, +2));
    }

    if (d_ptr->hoveredLinkOpt)
    {
        // TODO: make link highlighting style configurable

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 204, 160));

        for (const QRectF rect : d_ptr->hoveredLinkOpt->geometry())
            painter->drawRect(rect.adjusted(-0, -2, +0, +2));
    }

    painter->restore();
}

void DocumentPageItem::ResetSelection()
{
    d_ptr->textSelector.resetSelection();
    update();
}

void DocumentPageItem::AppendSelection(const DocumentSelection::Option& option)
{
    d_ptr->textSelector.appendSelection(option);
    update();
}

void DocumentPageItem::UpdateLastSelection(const DocumentSelection::Option& option)
{
    d_ptr->textSelector.updateLastSelection(option);
    update();
}

QString DocumentPageItem::GetSelectedText() const
{
    return d_ptr->textSelector.text();
}

int DocumentPageItem::Number() const
{
    return d_ptr->number;
}

void DocumentPageItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    updateCursorShape(event->pos());
    QGraphicsItem::hoverMoveEvent(event);
}

void DocumentPageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    d_ptr->hoveredLinkOpt.reset();
    unsetCursor();
    QGraphicsItem::hoverLeaveEvent(event);
}

void DocumentPageItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    updateCursorShape(event->pos());
    QGraphicsItem::mouseMoveEvent(event);
}

void DocumentPageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (const auto link = d_ptr->document->getLink(d_ptr->number, event->pos()); link)
        d_ptr->feedback->linkPressed(*link);

    QGraphicsItem::mouseReleaseEvent(event);
}

void DocumentPageItem::updateLinkHover(QPointF pos)
{
    const auto equals = [](const std::optional<DocumentLink>& first, const std::optional<DocumentLink>& second) -> bool
    {
        // TODO: optimize
        const QList<QRectF> f = first.has_value() ? first->geometry() : QList<QRectF> {};
        const QList<QRectF> s = second.has_value() ? second->geometry() : QList<QRectF> {};
        return f == s;
    };

    const std::optional<DocumentLink> link = d_ptr->document->getLink(d_ptr->number, pos);

    if (equals(d_ptr->hoveredLinkOpt, link))
        return;

    qDebug() << "Link hover:" << link.has_value();
    d_ptr->hoveredLinkOpt = link;
    update();
}

void DocumentPageItem::updateCursorShape(const QPointF pos)
{
    updateLinkHover(pos);

    if (d_ptr->hoveredLinkOpt)
    {
        setCursor(Qt::CursorShape::PointingHandCursor);
    }
    else if (d_ptr->document->hasText(d_ptr->number, pos))
    {
        setCursor(Qt::CursorShape::IBeamCursor);
    }
    else
    {
        unsetCursor();
    }
}
