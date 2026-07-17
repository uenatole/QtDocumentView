// Created by a.kutnyak@rit.va on 2026-07-16.

#include "DocumentLayout.h"

auto DocumentLayout::GetPage(const int page) const -> std::shared_ptr<const PageLayout>
{
    const auto objectPtr = m_pages.object(page);
    return objectPtr ? *objectPtr : nullptr;
}

auto DocumentLayout::InsertPage(const int page, const std::shared_ptr<const PageLayout>& object) -> void
{
    m_pages.insert(page, new std::shared_ptr(object));
}

auto DocumentLayout::SetMemoryLimit(const std::optional<std::size_t> bytes) -> void
{
    m_pages.setMaxCost(bytes ? static_cast<qsizetype>(*bytes) : std::numeric_limits<qsizetype>::max());
}

auto DocumentLayout::Reset() -> void
{
    m_pages.clear();
}
