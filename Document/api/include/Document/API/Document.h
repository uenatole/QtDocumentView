#pragma once

#include <QFuture>
#include <QRectF>
#include <QImage>

#include "DocumentLink.h"

struct Document
{
    virtual ~Document() = default;

    virtual auto pageCount() const -> std::size_t = 0;
    virtual auto pagePointSize(int page) const -> QSizeF = 0;

    // TODO: write comments
    virtual auto textReady(int page) const -> bool = 0;
    virtual auto forceTextReadiness(int page) const -> QFuture<void> = 0;

    // TODO: make async
    virtual auto text(int page, int from = 0, int count = -1) const -> QString = 0;
    virtual auto textBoxes(int page, int from = 0, int count = -1) const -> QList<QRectF> = 0;

    virtual auto render(int page, qreal scale) const -> QFuture<QImage> = 0;

    virtual auto links(int page) const -> QList<DocumentLink> = 0;
};
