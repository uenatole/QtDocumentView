// Created by a.kutnyak@rit.va on 2026-07-16.

#pragma once

#include <QCache>

#include "PageLayout.h"

// TODO: Pimpl it!
struct DocumentLayout
{
    auto GetPage(int) const -> std::shared_ptr<const PageLayout>;
    auto InsertPage(int, const std::shared_ptr<const PageLayout>&) -> void;

    auto SetMemoryLimit(std::optional<std::size_t> bytes) -> void;
    auto Reset() -> void;

private:
    QCache<int, std::shared_ptr<const PageLayout>> m_pages;
};
