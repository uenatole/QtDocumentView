#include "DocumentPageItem.h"

#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>

#include <Document/API/DocumentFacade.h>
#include <Document/API/DocumentParser.h>

struct DocumentPageItem::Private
{
    friend class DocumentPageItem;

    Private(const std::shared_ptr<DocumentFacade>& document, Feedback* feedback, const int number)
        : document(document)
        , feedback(feedback)
        , textSelection(document->selection())
        , number(number)
        , pointSize(document->pageSize(number))
    {}

private:
    std::shared_ptr<DocumentFacade> const document;
    Feedback* const feedback;
    const std::unique_ptr<DocumentSelection> textSelection;

    const int number;
    const QSizeF pointSize;

    QRectF selectionRect;
    std::optional<DocumentLink> currentLink;
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

    if (const QRectF rect = d_ptr->selectionRect; !rect.isNull())
    {
        // TODO: make text selection style configurable

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(206, 235, 249, 200));

        for (const QList<QRectF> geometries = d_ptr->textSelection->geometry(); const QRectF& geometry : geometries)
            painter->drawRect(geometry.adjusted(-0, -2, +0, +2));
    }

    if (d_ptr->currentLink)
    {
        // TODO: make link highlighting style configurable

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 204, 160));

        for (const QRectF rect : d_ptr->currentLink->geometry())
            painter->drawRect(rect.adjusted(-0, -2, +0, +2));
    }

    painter->restore();
}

void DocumentPageItem::SetSelectionRect(const QRectF& rect)
{
    if (rect != d_ptr->selectionRect)
    {
        d_ptr->selectionRect = rect;

        const auto idTmp = d_ptr->textSelection->hash();
        d_ptr->textSelection->configure(d_ptr->number, rect);

        if (idTmp != d_ptr->textSelection->hash())
        {
            update();
        }
    }
}

QString DocumentPageItem::GetSelectedText() const
{
    return d_ptr->textSelection->text();
}

int DocumentPageItem::Number() const
{
    return d_ptr->number;
}

void DocumentPageItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    updateCurrentLink(d_ptr->document->getLink(d_ptr->number, event->pos()));
    updateCursorShape(event->pos());
    QGraphicsItem::hoverMoveEvent(event);
}

void DocumentPageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    updateCurrentLink(std::nullopt);
    updateCursorShape();
    QGraphicsItem::hoverLeaveEvent(event);
}

void DocumentPageItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    updateCurrentLink(d_ptr->document->getLink(d_ptr->number, event->pos()));
    updateCursorShape(event->pos());
    QGraphicsItem::mouseMoveEvent(event);
}

void DocumentPageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (const auto link = d_ptr->document->getLink(d_ptr->number, event->pos()); link)
        d_ptr->feedback->linkPressed(*link);

    updateCursorShape(event->pos());
    QGraphicsItem::mouseReleaseEvent(event);
}

void DocumentPageItem::updateCurrentLink(const std::optional<DocumentLink>& link)
{
    const auto equals = [](const std::optional<DocumentLink>& first, const std::optional<DocumentLink>& second) -> bool
    {
        // TODO: optimize
        const QList<QRectF> f = first.has_value() ? first->geometry() : QList<QRectF> {};
        const QList<QRectF> s = second.has_value() ? second->geometry() : QList<QRectF> {};
        return f == s;
    };

    if (equals(d_ptr->currentLink, link))
        return;

    d_ptr->currentLink = link;
    update();
}

void DocumentPageItem::updateCursorShape(std::optional<QPointF> pos)
{
    if (!pos)
    {
        unsetCursor();
    }
    else if (d_ptr->currentLink)
    {
        setCursor(Qt::CursorShape::PointingHandCursor);
    }
    else if (d_ptr->document->hasText(d_ptr->number, *pos))
    {
        setCursor(Qt::CursorShape::IBeamCursor);
    }
    else
    {
        unsetCursor();
    }
}
