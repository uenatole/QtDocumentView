#include "DocumentSelector.h"

#include <QMouseEvent>
#include <QApplication>

#include "DocumentView.h"
#include "DocumentPageItem.h"

#include <QDateTime>

DocumentSelector::DocumentSelector(DocumentView* parent)
    : QObject(parent)
    , m_view(parent)
{
    m_view->viewport()->installEventFilter(this);

    m_clickTimer.setSingleShot(true);
    m_clickTimer.setInterval(QApplication::doubleClickInterval());
    connect(&m_clickTimer, &QTimer::timeout, this, [this] { m_clickCount = 0; });
}

bool DocumentSelector::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_view->viewport())
        return QObject::eventFilter(object, event);

    if (const auto mouse = dynamic_cast<QMouseEvent*>(event); mouse)
    {
        const auto pos = mouse->pos();

        if (mouse->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton)
        {
            if (!m_start)
                onPressed(pos);
        }
        else if (mouse->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton)
        {
            if (m_start)
                onReleased(pos);

            handleClick(pos);
        }
        else if (mouse->type() == QEvent::MouseMove)
        {
            if (m_start)
                onMoved(pos);
        }
    }

    return QObject::eventFilter(object, event);
}

void DocumentSelector::handleClick(QPoint point)
{
    m_clickCount++;
    m_clickTimer.start();

    if (m_clickCount == 2)
    {
        onDoubleClicked(point);
    }
    else if (m_clickCount == 3)
    {
        onTripleClicked(point);
    }
}

void DocumentSelector::onPressed(const QPoint point)
{
    m_start = m_view->mapToScene(point);

    for (const auto item : m_view->items())
        if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            page->SelectLines({});
}

void DocumentSelector::onReleased(QPoint)
{
    m_start = std::nullopt;
}

void DocumentSelector::onMoved(const QPoint point) const
{
    // NOTE: this progressive selection method is quite inefficient due to selection from the very beginning on every update.
    // TODO: provide stateful selection API to make it truly progressive.

    const auto first = *m_start;
    const auto second = m_view->mapToScene(point);
    const auto selectionRect = QRectF(first, second).normalized();

    for (const auto item : m_view->items())
        if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
        {
            const QRectF sceneIntersectionRect = selectionRect.intersected(page->sceneBoundingRect());

            if (sceneIntersectionRect.isNull())
                continue;

            const QRectF pageIntersectionRect = page->mapRectFromScene(sceneIntersectionRect);
            page->SelectLines(pageIntersectionRect);
        }
}

void DocumentSelector::onDoubleClicked(QPoint point) const
{
    for (const auto item : m_view->items())
        if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
        {
            const auto location = m_view->mapToScene(point);
            if (page->sceneBoundingRect().contains(location))
            {
                page->SelectWord(page->mapFromScene(location));
                break;
            }
        }
}

void DocumentSelector::onTripleClicked(QPoint point) const
{
    for (const auto item : m_view->items())
        if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
        {
            const auto location = m_view->mapToScene(point);
            if (page->sceneBoundingRect().contains(location))
            {
                page->SelectLine(page->mapFromScene(location));
                break;
            }
        }
}
