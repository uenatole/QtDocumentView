#pragma once

#include <optional>
#include <QImage>

struct Document;

struct DocumentRenderFeedback
{
    virtual ~DocumentRenderFeedback() = default;

    virtual bool isActual(int page) const = 0;
    virtual void imageReady(int page) const = 0;
};

struct DocumentRenderer
{
    virtual ~DocumentRenderer() = default;

    virtual auto setDocument(std::shared_ptr<const Document> document) -> void = 0;

    /* Current return type is the result of bad design,
     * based on a specific use case:
     *
     * DocumentPageItem expects to get the previous render result
     * from the renderer because it calls a method on paintEvent
     * and can't defer rendering using QFuture.
     *
     * The assumption that there is caching in the renderer is wrong,
     * so it should be refactored in the way
     * where response is stored on the user side.
     *
     * TODO: refactor it.
     */
    virtual auto requestPageRender(int page, qreal scale, DocumentRenderFeedback* feedback) const -> std::optional<QImage> = 0;
};
