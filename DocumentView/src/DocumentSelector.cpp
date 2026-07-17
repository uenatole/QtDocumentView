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

    auto processClick(const QPoint point) -> bool
    {
        clickCount = (clickCount + 1) % 4;
        clickTimer.start();

        if (clickCount == 2)
        {
            onDoubleClicked(point);
            return true;
        }

        if (clickCount == 3)
        {
            onTripleClicked(point);
            return true;
        }

        return false;
    }

    void onPressed(const QPoint point)
    {
        pressPosition = view->mapToScene(point);
        isDrag = false;

        if ((!clickTimer.isActive() || clickCount == 0) && !isPressedWithCtrl)
            clearSelection();
    }

    void onReleased(const QPoint point)
    {
        if (!isDrag)
        {
            processClick(point);
        }
        else
        {
            dragSelectedPages.clear();
            isDrag = false;
        }

        pressPosition = std::nullopt;
    }

    void onMoved(const QPoint point)
    {
        if (!pressPosition.has_value())
            return;

        if (!isDrag)
        {
            const auto currentPos = view->mapToScene(point);
            const qreal distance = QLineF(*pressPosition, currentPos).length();

            if (distance < QApplication::startDragDistance())
                return;

            isDrag = true;
            clickCount = 0;
            clickTimer.stop();
        }

        // NOTE: this progressive selection method is quite inefficient due to selection from the very beginning on every update.
        // TODO: provide stateful selection API to make it truly progressive.

        const auto first = *pressPosition;
        const auto second = view->mapToScene(point);
        const auto selectionRect = QRectF(first, second).normalized();

        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const QRectF sceneIntersectionRect = selectionRect.intersected(page->sceneBoundingRect());

                if (sceneIntersectionRect.isNull())
                    continue;

                const QRectF pageIntersectionRect = page->mapRectFromScene(sceneIntersectionRect);

                if (dragSelectedPages.contains(page->Number()))
                {
                    page->UpdateLastSelection(DocumentSelection::Lines { pageIntersectionRect });
                }
                else
                {
                    page->AppendSelection(DocumentSelection::Lines { pageIntersectionRect });
                    dragSelectedPages.insert(page->Number());
                }
            }
    }

    void onDoubleClicked(const QPoint point) const
    {
        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const auto location = view->mapToScene(point);
                if (page->sceneBoundingRect().contains(location))
                {
                    page->AppendSelection(DocumentSelection::Word { page->mapFromScene(location) });
                    break;
                }
            }
    }

    void onTripleClicked(const QPoint point) const
    {
        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
            {
                const auto location = view->mapToScene(point);
                if (page->sceneBoundingRect().contains(location))
                {
                    page->AppendSelection(DocumentSelection::Line { page->mapFromScene(location) });
                    break;
                }
            }
    }

    void clearSelection() const
    {
        for (const auto item : view->items())
            if (const auto page = dynamic_cast<DocumentPageItem*>(item); page)
                page->ResetSelection();
    }

    DocumentView* const view;

    QTimer clickTimer;
    int clickCount = 0;

    std::optional<QPointF> pressPosition;
    bool isPressedWithCtrl = false;

    bool isDrag = false;
    QSet<int> dragSelectedPages;
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
            d->isPressedWithCtrl = mouse->modifiers() & Qt::ControlModifier;
            d->onPressed(pos);
        }
        else if (mouse->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton)
        {
            d->onReleased(pos);
        }
        else if (mouse->type() == QEvent::MouseMove)
        {
            d->onMoved(pos);
        }
    }

    return QObject::eventFilter(object, event);
}
