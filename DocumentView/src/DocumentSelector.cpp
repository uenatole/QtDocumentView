#include "DocumentSelector.h"

#include <QMouseEvent>
#include <QApplication>

#include "DocumentView.h"
#include "DocumentPageItem.h"

#include <QDateTime>

struct DocumentSelector::Private
{
    explicit Private(DocumentView* view)
        : view(view)
    {}

    auto tryHandleClick(QPoint point) -> bool
    {
        clickCount++;
        clickTimer.start();

        if (clickCount == 2)
        {
            onDoubleClicked(point);
            return true;
        }
        else if (clickCount == 3)
        {
            onTripleClicked(point);
            return true;
        }

        return false;
    }

    void onPressed(QPoint point)
    {
        dragStart = view->mapToScene(point);

        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
                page->SelectLines({});
    }

    void onReleased(QPoint point)
    {
        dragStart = std::nullopt;
    }

    void onMoved(QPoint point)
    {
        // NOTE: this progressive selection method is quite inefficient due to selection from the very beginning on every update.
        // TODO: provide stateful selection API to make it truly progressive.

        const auto first = *dragStart;
        const auto second = view->mapToScene(point);
        const auto selectionRect = QRectF(first, second).normalized();

        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const QRectF sceneIntersectionRect = selectionRect.intersected(page->sceneBoundingRect());

                if (sceneIntersectionRect.isNull())
                    continue;

                const QRectF pageIntersectionRect = page->mapRectFromScene(sceneIntersectionRect);
                page->SelectLines(pageIntersectionRect);
            }
    }

    void onDoubleClicked(QPoint point)
    {
        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const auto location = view->mapToScene(point);
                if (page->sceneBoundingRect().contains(location))
                {
                    page->SelectWord(page->mapFromScene(location));
                    break;
                }
            }
    }

    void onTripleClicked(QPoint point)
    {
        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const auto location = view->mapToScene(point);
                if (page->sceneBoundingRect().contains(location))
                {
                    page->SelectLine(page->mapFromScene(location));
                    break;
                }
            }
    }

    DocumentView* const view;

    QTimer clickTimer;
    int clickCount = 0;

    std::optional<QPointF> dragStart;
};

DocumentSelector::DocumentSelector(DocumentView* parent)
    : QObject(parent)
    , d(std::make_unique<Private>(parent))
{
    d->view->viewport()->installEventFilter(this);

    d->clickTimer.setSingleShot(true);
    d->clickTimer.setInterval(QApplication::doubleClickInterval());
    connect(&d->clickTimer, &QTimer::timeout, this, [this] { d->clickCount = 0; });
}

DocumentSelector::~DocumentSelector() = default;

bool DocumentSelector::eventFilter(QObject* object, QEvent* event)
{
    if (object != d->view->viewport())
        return QObject::eventFilter(object, event);

    if (const auto mouse = dynamic_cast<QMouseEvent*>(event); mouse)
    {
        const auto pos = mouse->pos();

        if (mouse->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton)
        {
            if (!d->dragStart)
                d->onPressed(pos);
        }
        else if (mouse->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton)
        {
            d->tryHandleClick(pos);

            if (d->dragStart)
                d->onReleased(pos);
        }
        else if (mouse->type() == QEvent::MouseMove)
        {
            if (d->dragStart)
                d->onMoved(pos);
        }
    }

    return QObject::eventFilter(object, event);
}
