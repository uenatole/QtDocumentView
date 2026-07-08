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

    auto update(const DocumentSelection::Option& option, const bool append)
    {
        if (std::holds_alternative<DocumentSelection::None>(option))
        {
            m_selections.clear();
            return;
        }

        if (!append)
            m_selections.clear();

        // Создаем новое выделение
        auto newSelection = m_document->selection();
        newSelection->configure(m_page, option);

        if (m_selections.empty()) {
            m_selections.push_back(std::move(newSelection));
            return;
        }

        auto [newStart, newEnd] = newSelection->range();

        const auto firstOverlap = std::lower_bound(
            m_selections.begin(),
            m_selections.end(),
            newStart,
            [](const auto& sel, std::size_t value) {
                return sel->range().second < value;
            }
        );

        const auto lastOverlap = std::upper_bound(
            firstOverlap,
            m_selections.end(),
            newEnd,
            [](std::size_t value, const auto& sel) {
                return value < sel->range().first;
            }
        );

        if (firstOverlap == lastOverlap)
        {
            const auto insertPos = std::lower_bound(
                m_selections.begin(),
                m_selections.end(),
                newStart,
                [](const auto& sel, std::size_t value) {
                    return sel->range().first < value;
                }
            );
            m_selections.insert(insertPos, std::move(newSelection));
            return;
        }

        auto mergedStart = std::min(newStart, (*firstOverlap)->range().first);
        auto mergedEnd = std::max(newEnd, (*std::prev(lastOverlap))->range().second);

        auto mergedSelection = m_document->selection();
        mergedSelection->configure(m_page, {mergedStart, mergedEnd});

        *firstOverlap = std::move(mergedSelection);
        m_selections.erase(std::next(firstOverlap), lastOverlap);
    }

private:
    const int m_page;
    const std::shared_ptr<const DocumentFacade> m_document;
    std::vector<std::unique_ptr<DocumentSelection>> m_selections;
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

void DocumentPageItem::UpdateSelection(const DocumentSelection::Option &option,
                                       const bool append)
{
    d_ptr->textSelector.update(option, append);
    update(); // TODO: return bool (should update) from PageTextSelector::update
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
