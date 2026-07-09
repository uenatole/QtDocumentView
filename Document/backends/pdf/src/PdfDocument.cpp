#include "PdfDocument.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QPdfDocument>
#include <QPdfLinkModel>
#include <QElapsedTimer>

struct PdfDocument::Private
{
    QPdfDocument doc;
};

PdfDocument::PdfDocument()
    : d(std::make_unique<Private>())
{}

PdfDocument::~PdfDocument() = default;

void PdfDocument::load(const QString& path)
{
    d->doc.load(path);
}

auto PdfDocument::pageCount() const -> std::size_t
{
    return d->doc.pageCount();
}

auto PdfDocument::pagePointSize(int page) const -> QSizeF
{
    return d->doc.pagePointSize(page);
}
auto PdfDocument::textReady(int) const -> bool
{
    return true;
}

auto PdfDocument::forceTextReadiness(int) const -> QFuture<void>
{
    return QtFuture::makeReadyVoidFuture();
}

auto PdfDocument::text(int page, int from, int count) const -> QString
{
    return d->doc.getTextContentsAtIndex(page, from, from + count);
}

auto PdfDocument::textBoxes(int page, int from, int count) const -> QList<QRectF>
{
    return d->doc.getCharGeometryAtIndex(page, from, from + count);
}

auto PdfDocument::render(int page, qreal scale) const -> QFuture<QImage>
{
    return QtConcurrent::run(
        [&document=d->doc, page, scale](QPromise<QImage>& promise)
        {
            struct PromiseCancel : QPdfDocument::ICancel {
                explicit PromiseCancel(QPromise<QImage>& promise) : m_promise(promise) {}
                bool isCancelled() final
                {
                    return m_promise.isCanceled();
                }

            private:
                QPromise<QImage>& m_promise;
            };

            const auto pointSize = document.pagePointSize(page);
            const auto renderSize = pointSize * scale;
            const auto size = renderSize.toSize();

            const auto cancel = std::make_unique<PromiseCancel>(promise);

            QElapsedTimer timer;
            timer.start();
            const QImage result = document.render2(page, size, cancel.get());

            promise.addResult(result);
        }
    );
}

auto PdfDocument::links(int page) const -> QList<DocumentLink>
{
    QList<DocumentLink> Links;

    // TODO: get rid of QPdfLinkModel
    {
        QPdfLinkModel links;
        links.setDocument(&d->doc);
        links.setPage(page);

        for (int i = 0; i < links.rowCount(QModelIndex()); ++i)
        {
            QVariant variant = links.data(links.index(i), static_cast<int>(QPdfLinkModel::Role::Link));
            QPdfLink link = variant.value<QPdfLink>();

            if (link.url().isValid())
                Links.append({ page, link.rectangles(), DocumentLink::Url(link.url())});
            else
                Links.append({ page, link.rectangles(), DocumentLink::Jump(link.page(), link.zoom(), link.location() )});
        }
    }

    return Links;
}
