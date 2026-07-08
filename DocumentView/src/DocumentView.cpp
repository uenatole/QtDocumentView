#include "DocumentView.h"

#include <QDesktopServices>
#include <QGraphicsScene>
#include <QGraphicsEffect>
#include <QWheelEvent>

#include <Document/API/DocumentFacade.h>
#include <Document/API/DocumentParser.h>
#include <Document/API/DocumentRenderer.h>

#include "DocumentPageItem.h"

struct RenderFeedback : DocumentRenderFeedback
{
    explicit RenderFeedback(DocumentView* view) : _view(view){}

    [[nodiscard]] bool isActual(const int page) const final
    {
        const auto item = _view->page(page);

        const QRect portRect = _view->viewport()->rect();
        const QRectF sceneRect = _view->mapToScene(portRect).boundingRect();
        const QRectF itemRect = item->mapRectFromScene(sceneRect);

        return itemRect.intersects(item->boundingRect());
    }

    void imageReady(const int page) const final
    {
        const auto item = _view->page(page);
        item->update();
    }

private:
    DocumentView* _view;
};

struct PageItemFeedback : DocumentPageItem::Feedback
{
    explicit PageItemFeedback(DocumentView* view) : _view(view){}

    void linkPressed(const DocumentLink& link) override
    {
        if (const auto contents = std::get_if<DocumentLink::Url>(&link.contents()); contents)
        {
            QDesktopServices::openUrl(contents->url());
        }

        if (const auto contents = std::get_if<DocumentLink::Jump>(&link.contents()); contents)
        {
            const auto number = contents->destinationPage();
            const auto location = contents->destinationLocation();
            const auto zoom = contents->destinationZoom();

            QTransform t = _view->transform();
            t.setMatrix(
                zoom, t.m12(), t.m13(),
                t.m21(), zoom, t.m23(),
                t.m31(), t.m32(), t.m33()
            );

            const auto page = dynamic_cast<DocumentPageItem*>(_view->page(number));
            _view->setTransform(t);
            page->ensureVisible({ location, location });
        }
    }

private:
    DocumentView* const _view;
};

struct DocumentView::Private
{
    explicit Private(DocumentView* q)
        : feedback(new PageItemFeedback(q))
    {}

    const std::unique_ptr<DocumentPageItem::Feedback> feedback;

    std::shared_ptr<DocumentFacade> document;
    QMap<int, DocumentPageItem*> pages;
};

DocumentView::DocumentView(QWidget* parent)
    : QGraphicsView(parent)
    , d(new Private(this))
{}

DocumentView::~DocumentView(){}

void DocumentView::setDocument(const std::shared_ptr<DocumentFacade>& document)
{
    d->document = document;
    d->document->setRenderFeedback(new RenderFeedback(this));

    auto* scene = new QGraphicsScene();
    scene->setBackgroundBrush(palette().brush(QPalette::Dark));

    constexpr auto documentMargins = 6;

    qreal yCursor = documentMargins;
    qreal maxPageWidth = std::numeric_limits<qreal>::min();

    for (int page = 0; page < document->pageCount(); ++page)
    {
        QSizeF pagePointSize = document->pageSize(page);

        const auto item = new DocumentPageItem(document, d->feedback.get(), page);
        item->setPos(documentMargins, yCursor);

        yCursor += pagePointSize.height() + documentMargins;
        maxPageWidth = std::max(maxPageWidth, pagePointSize.width());

        // NOTE: According to the performance profiler, this causes large lags when scaling large
        // const auto shadowEffect = new QGraphicsDropShadowEffect();
        // shadowEffect->setBlurRadius(10.0);
        // shadowEffect->setColor(QColor(0, 0, 0, 150));
        // shadowEffect->setOffset(2, 2);
        // item->setGraphicsEffect(shadowEffect);

        scene->addItem(item);
        d->pages.insert(page, item);
    }

    setScene(scene);
    setSceneRect(0, 0, maxPageWidth + 2 * documentMargins, yCursor);

    centerOn(0, 0);
    setTransformationAnchor(AnchorUnderMouse);
}

QString DocumentView::getSelectedText() const
{
    QString text;

    for (const QGraphicsItem* page : d->pages.asKeyValueRange() | std::views::values)
        text += dynamic_cast<const DocumentPageItem*>(page)->GetSelectedText();

    return text.trimmed();
}

QGraphicsItem* DocumentView::page(int i) const
{
    return d->pages[i];
}
