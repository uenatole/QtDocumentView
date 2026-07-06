#include "StandardDocumentRenderer.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QTimer>

#include <Document/API/Document.h>

#include "RenderCache.h"

namespace
{
    struct RenderRequest
    {
        int Page;
        qreal Scale;
        DocumentRenderFeedback* Feedback {};

        bool operator==(const RenderRequest& other) const
        {
            return Page == other.Page && qFuzzyCompare(Scale, other.Scale);
        }
    };

    struct RenderState
    {
        RenderRequest Request;
        QFuture<void> Future;

        RenderState(const RenderRequest& parameters, QFuture<void> future)
            : Request(parameters)
            , Future(std::move(future))
        {}

        ~RenderState()
        {
            Future.cancelChain();
        }
    };
}

struct StandardDocumentRenderer::Private
{
    explicit Private()
    {
        dequeueDelayTimer.setSingleShot(true);
        dequeueDelayTimer.setInterval(50);
        QObject::connect(&dequeueDelayTimer, &QTimer::timeout, [this]{ tryDequeueRenderRequest(); });
    }

    std::optional<QImage> request(const int page, const qreal scale, DocumentRenderFeedback* feedback)
    {
        if (const QImage* image = renderCache.object(page, scale); image)
        {
            qDebug() << "Cache hit: page =" << page << "scale =" << scale;
            return *image;
        }

        RenderRequest request { page, scale, feedback };
        const std::optional<QImage> nearestImage = findNearestImage(page, scale);

        // Check active render request for duplication
        if (renderState)
        {
            if (renderState->Request == request)
            {
                return nearestImage;
            }

            if (renderState->Request.Page == request.Page)
            {
                renderState.reset();
            }
        }

        // Check pending render requests for duplication
        for (RenderRequest& other : requests)
        {
            if (other.Page == request.Page)
            {
                other.Scale = request.Scale;
                return nearestImage;
            }
        }

        // Enqueue new request
        enqueueRenderRequest(std::move(request));

        // Dequeue it delayed to start render and let for some requests to be outdated
        if (!renderState)
            tryDequeueRenderRequestDelayed();

        return nearestImage;
    }

private:
    std::optional<QImage> findNearestImage(const int page, const qreal scale) const
    {
        if (auto* image = renderCache.nearestObject(page, scale); image)
            return *image;

        return std::nullopt;
    }

    void enqueueRenderRequest(RenderRequest&& request)
    {
        requests.emplace_back(std::move(request));
    }

    void tryDequeueRenderRequestDelayed()
    {
        dequeueDelayTimer.start();
    }

    void tryDequeueRenderRequest()
    {
        if (renderState && !renderState->Request.Feedback->isActual(renderState->Request.Page))
        {
            renderState.reset();
        }

        // Erase unactual requests
        const auto firstActualIt = std::find_if(requests.begin(), requests.end(), [](const RenderRequest& request)
        {
            return request.Feedback->isActual(request.Page);
        });

        const auto prevSize = requests.size();
        requests.erase(requests.begin(), firstActualIt);

        if (const auto diff = prevSize - requests.size(); diff) qDebug() << "Erased" << diff << "elements";

        if (requests.empty()) return;

        // Take first request in queue
        RenderRequest request = std::move(requests.front());
        requests.pop_front();

        QFuture<void> future =
            document->render(request.Page, request.Scale * pixelRatio)
            .then(QThread::currentThread(), [this, request](const QImage& image){
                (void) renderCache.insert(request.Page, request.Scale, new QImage(image));
                request.Feedback->imageReady(request.Page);

                renderState.reset();
                tryDequeueRenderRequest();
            });

        renderState.emplace(request, future);
    }

    friend class StandardDocumentRenderer;

    std::shared_ptr<const Document> document;

    qreal pixelRatio = 1.0;

    QTimer dequeueDelayTimer;

    mutable RenderCache renderCache;

    std::list<RenderRequest> requests;
    std::optional<RenderState> renderState;
};

StandardDocumentRenderer::StandardDocumentRenderer()
    : d(std::make_unique<Private>())
{
    setRenderCacheLimit(512 /*MiB*/ * 1024 /*KiB*/ * 1024 /*B*/);
}

StandardDocumentRenderer::~StandardDocumentRenderer() = default;

auto StandardDocumentRenderer::setPixelRatio(qreal ratio) const -> void
{
    // TODO: invalidate cache (?) or take ratio in account with {scale} (!)
    d->pixelRatio = ratio;
}

auto StandardDocumentRenderer::setRenderCacheLimit(qreal bytes) const -> void
{
    d->renderCache.setLimit(bytes);
}

auto StandardDocumentRenderer::setRenderDelay(int ms) const -> void
{
    d->dequeueDelayTimer.setInterval(ms);
}

auto StandardDocumentRenderer::setDocument(std::shared_ptr<const Document> document) -> void
{
    // Reset active state
    d->dequeueDelayTimer.stop();
    d->renderState.reset();
    d->requests.clear();
    d->renderCache.clear();

    d->document = document;
}

auto StandardDocumentRenderer::requestPageRender(int page, qreal scale, DocumentRenderFeedback* feedback) const -> std::optional<QImage>
{
    return d->request(page, scale, feedback);
}
