// Created by a.kutnyak@rit.va on 2026-07-16.

#include <memory>
#include <optional>

#include "layout/PageLayout.h"

auto ParsePageLayout(const QString& chars, const QList<QRectF>& boxes) -> std::shared_ptr<PageLayout>
{
    Q_ASSERT(chars.size() == boxes.size());

    auto layout = std::make_shared<PageLayout>();

    if (boxes.isEmpty())
        return layout;

    LineLayout line;
    line.FirstCharIndex = 0;
    line.Geometry = boxes[0];

    std::optional<std::size_t> wordBeginOpt = std::nullopt;

    if (!chars[0].isSpace()) {
        wordBeginOpt = boxes[0].left();
    }

    line.Chars.push_back(boxes[0]);

    for (int i = 1; i < chars.size(); ++i)
    {
        const auto& chr = chars[i];
        const auto& box = boxes[i];

        const auto isAdjacent = [](const QRectF& r1, const QRectF& r2) -> bool {
            // TODO: add epsilon
            return r2.top() <= r1.bottom() && r2.bottom() >= r1.top();
        };

        if (isAdjacent(line.Geometry, box)) {
            line.Geometry |= box;
        }
        else {
            layout->Lines.emplace_back(std::move(line));

            line.Geometry = box;
            line.FirstCharIndex = i;
        }

        line.Chars.emplace_back(box);

        if (!chr.isSpace() && !wordBeginOpt) {
            wordBeginOpt = box.left();
        }
        if (chr.isSpace() && wordBeginOpt) {
            line.Words.emplace_back(*wordBeginOpt, boxes[i - 1].right());
            wordBeginOpt = std::nullopt;
        }
    }

    if (wordBeginOpt) {
        line.Words.emplace_back(*wordBeginOpt, boxes.last().right());
    }

    if (!line.Chars.isEmpty()) {
        layout->Lines.emplace_back(std::move(line));
    }

    return layout;
}
