#include "RenderCache.h"

#include "Utils.h"

RenderCache::RenderCache()
{
    _storage.setOnEraseFn([this](const std::pair<int, qreal> &key) {
        _keySets[key.first].erase(key.second);
    });
}

auto RenderCache::object(int page, qreal scale) const -> QImage*
{
    return _storage.object({page, scale});
}

auto RenderCache::nearestObject(int page, const qreal targetScale) const -> QImage*
{
    const auto& scales = _keySets[page];
    const auto closestScaleIt = closest_element(scales, targetScale);

    if (closestScaleIt == scales.end())
        return nullptr;

    return _storage.object({page, *closestScaleIt});
}

auto RenderCache::insert(int page, qreal scale, QImage* image) const -> bool
{
    if (const bool inserted = _storage.insert({page, scale}, image, image->sizeInBytes()); Q_LIKELY(inserted))
    {
        _keySets[page].insert(scale);
        return true;
    }
    return false;
}

auto RenderCache::setLimit(const std::size_t bytes) const -> void
{
    _storage.setMaxCost(bytes);
}

auto RenderCache::clear() -> void
{
    _storage.clear();
    _keySets.clear();
}
