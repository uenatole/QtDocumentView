#include "OcrDocument.h"

#include "OcrEngine.h"

struct OcrDocument::Private
{
    // NOTE: thread safety?
    std::shared_ptr<const Document> source;
    std::unique_ptr<OcrEngine> engine;

    mutable QMutex cacheMutex;
    mutable QHash<int, OcrEngine::Result> cache;
    mutable QHash<int, QFuture<void>> cachePendingFutures; // TODO: prepare for async (store QFuture<OcrEngine::Result>)

    auto load(const int page) const -> QFuture<void>
    {
        QMutexLocker readLocker(&cacheMutex);

        if (cache.contains(page))
            return QtFuture::makeReadyVoidFuture();

        if (const auto it = cachePendingFutures.find(page); it != cachePendingFutures.end())
            return *it;

        constexpr double SCALE = 2;
        constexpr double INV_SCALE = 1 / SCALE;

        const auto pendingFuture = source->render(page, SCALE)
            .then([this](const QImage& image)
            {
                return engine->recognize(image);
            })
            .unwrap() // NOTE: ?
            .then([this, page](const OcrEngine::Result& result)
            {
                QMutexLocker insertLocker(&cacheMutex);

                // NOTE: upscale
                QList<QRectF> boxes = result.Boxes;
                for (auto& box : boxes)
                    box = QRectF(box.left() * INV_SCALE, box.top() * INV_SCALE, box.width() * INV_SCALE, box.height() * INV_SCALE);

                OcrEngine::Result value;
                value.Chars = result.Chars;
                value.Boxes = boxes;

                cache.insert(page, value);
                cachePendingFutures.remove(page);
            });

        cachePendingFutures.insert(page, pendingFuture);
        return pendingFuture;
    }

    // TODO: prepare for async version (without immediate)
    auto text(const int page, const int from, const int count) const -> QString
    {
        if (const auto textOpt = textImmediate(page, from, count); textOpt)
            return *textOpt;

        (void) load(page); // NOTE: trigger cache load.
        return {};  // NOTE: immediate mode; client must use Document::textReady to be sure that result is ready.
    }

    // TODO: prepare for async version (without immediate)
    auto textBoxes(const int page, const int from, const int count) const -> QList<QRectF>
    {
        if (const auto textBoxesOpt = textBoxesImmediate(page, from, count); textBoxesOpt)
            return *textBoxesOpt;

        (void) load(page); // NOTE: trigger cache load.
        return {};  // NOTE: immediate mode; client must use Document::textReady to be sure that result is ready.
    }

private:
    auto textImmediate(const int page, const int from, const int count) const -> std::optional<QString>
    {
        QMutexLocker readLocker(&cacheMutex);
        if (const auto it = cache.find(page); it != cache.end())
        {
            const auto& [Chars, _] = *it;
            return Chars.mid(from, count);
        }

        return std::nullopt;
    }

    auto textBoxesImmediate(const int page, const int from, const int count) const -> std::optional<QList<QRectF>>
    {
        QMutexLocker readLocker(&cacheMutex);
        if (const auto it = cache.find(page); it != cache.end())
        {
            const auto& [_, Boxes] = *it;
            return Boxes.mid(from, count);
        }

        return std::nullopt;
    }
};

OcrDocument::OcrDocument()
    : d(std::make_unique<Private>())
{}

OcrDocument::~OcrDocument() = default;

void OcrDocument::setSource(const std::shared_ptr<const Document>& source)
{
    d->source = source;
    d->cache.clear();
}

void OcrDocument::setEngine(std::unique_ptr<OcrEngine>&& engine)
{
    d->engine = std::move(engine);
}

auto OcrDocument::pageCount() const -> std::size_t
{
    return d->source->pageCount();
}

auto OcrDocument::pagePointSize(int page) const -> QSizeF
{
    return d->source->pagePointSize(page);
}

auto OcrDocument::textReady(int page) const -> bool
{
    return d->cache.contains(page);
}

auto OcrDocument::forceTextReadiness(int page) const -> QFuture<void>
{
    return d->load(page);
}

auto OcrDocument::text(int page, int from, int count) const -> QString
{
    return d->text(page, from, count);
}

auto OcrDocument::textBoxes(int page, int from, int count) const -> QList<QRectF>
{
    return d->textBoxes(page, from, count);
}

auto OcrDocument::render(const int page, const qreal scale) const -> QFuture<QImage>
{
    return d->source->render(page, scale);
}

auto OcrDocument::links(const int) const -> QList<DocumentLink>
{
    return {}; // NOTE: ?
}
