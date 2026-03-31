#pragma once

#include <QImage>
#include <set>

#include "QCacheExt.h"

class RenderCache
{
public:
    RenderCache();

    auto object(int page, qreal scale) const -> QImage*;
    auto nearestObject(int page, qreal targetScale) const -> QImage*;

    auto insert(int page, qreal scale, QImage* image) const -> bool;

    auto setLimit(std::size_t bytes) const -> void;
    auto clear() -> void;

private:
    mutable QCacheExt<std::pair<int, qreal>, QImage> _storage;
    mutable QHash<int, std::set<qreal>> _keySets;
};
