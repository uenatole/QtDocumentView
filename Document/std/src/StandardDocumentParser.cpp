#include "StandardDocumentParser.h"

#include <QCache>
#include <QRectF>

#include <Document/API/Document.h>

namespace
{
    using LineIndices = std::pair<int32_t, int32_t>;
    using CharIndices = std::pair<int32_t, int32_t>;

    using InLineRange = std::pair<qreal, qreal>;

    struct LineLayout
    {
        int32_t FirstCharIndex;
        QRectF Geometry;
        QList<InLineRange> Chars;
        QList<InLineRange> Words;

        [[nodiscard]] auto indices() const -> CharIndices
        {
            return { FirstCharIndex, FirstCharIndex + Chars.size() };
        }

        [[nodiscard]] QList<InLineRange>::const_iterator findWordAt(const QPointF& pos) const
        {
            if (!Geometry.contains(pos))
                return Chars.constEnd();

            return std::find_if(Words.begin(), Words.end(), [pos](const InLineRange& word) {
                return word.first <= pos.x() && pos.x() <= word.second;
            });
        }

        [[nodiscard]] QList<InLineRange>::const_iterator findFirstCharCrossedBy(const QRectF& rect) const
        {
            if (rect.bottom() < Geometry.top() || rect.top() > Geometry.bottom())
                return Chars.constEnd();

            const auto it = std::lower_bound(Chars.constBegin(), Chars.constEnd(), rect.left(),
                [](const InLineRange& range, const qreal left) {
                    return charCenter(range) < left;
                });

            if (it != Chars.constEnd() && charCenter(*it) <= rect.right())
                return it;

            return Chars.constEnd();
        }

        [[nodiscard]] QList<InLineRange>::const_iterator findLastCharCrossedBy(const QRectF& rect) const
        {
            if (rect.bottom() < Geometry.top() || rect.top() > Geometry.bottom())
                return Chars.constEnd();

            auto it = std::upper_bound(Chars.constBegin(), Chars.constEnd(), rect.right(),
                [](const qreal right, const InLineRange& range) {
                    return right < charCenter(range);
                });

            if (it != Chars.constBegin())
            {
                --it;
                if (charCenter(*it) >= rect.left())
                    return it;
            }

            return Chars.constEnd();
        }

        [[nodiscard]] QRectF getGeometryByIndices(int begin, int end) const
        {
            if (begin >= indices().second || begin >= end)
                return {};

            begin = std::max(indices().first, begin);
            end = std::min(indices().second, end);

            if (begin >= end)
                return {};

            const qreal left = Chars[begin - indices().first].first;
            const qreal right = (end == indices().second)
                ? Geometry.right()
                : Chars[end - indices().first].first;

            QRectF subGeometry = Geometry;
            subGeometry.setLeft(left);
            subGeometry.setRight(right);

            return subGeometry;
        }

    private:
        static qreal charCenter(const InLineRange& charRange)
        {
            return (charRange.first + charRange.second) / 2.0;
        }
    };

    struct PageLayout
    {
        QList<LineLayout> Lines;
        QList<DocumentLink> Links;

        [[nodiscard]] QPair<QList<LineLayout>::const_iterator, QList<LineLayout>::const_iterator> findLinesCrossedBy(const QRectF& rect) const
        {
            auto first = std::partition_point(Lines.constBegin(), Lines.constEnd(),
                [&rect](const LineLayout& line) {
                    return line.Geometry.bottom() < rect.top();
                });

            auto last = std::partition_point(first, Lines.constEnd(),
                [&rect](const LineLayout& line) {
                    return line.Geometry.top() <= rect.bottom();
                });

            if (first == Lines.constEnd() || last == Lines.constBegin() || first >= last) {
                return { Lines.constEnd(), Lines.constEnd() };
            }

            --last;

            while (first <= last && !first->Geometry.intersects(rect)) {
                ++first;
            }

            if (first > last) {
                return { Lines.constEnd(), Lines.constEnd() };
            }

            while (last >= first && !last->Geometry.intersects(rect)) {
                --last;
            }

            return { first, last };
        }

        [[nodiscard]] QList<LineLayout>::const_iterator findLineAt(const QPointF point) const
        {
            const auto lineIt = std::lower_bound(Lines.begin(), Lines.end(), point, [](const LineLayout& line, const QPointF& p) -> bool {
                return line.Geometry.bottom() < p.y();
            });

            if (lineIt == Lines.end() || point.y() < lineIt->Geometry.top())
                return Lines.end();

            if (lineIt->Geometry.left() <= point.x() && point.x() <= lineIt->Geometry.right())
                return lineIt;

            return Lines.end();
        }
    };
}

struct StandardDocumentParser::Private
{
    QList<QRectF> getGeometryByIndices(const int page, const LineIndices& iLine, const CharIndices& iChar) const
    {
        const auto& [Lines, _] = getPageLayout(page);

        if (iLine.first == -1)
            return {};

        QList<QRectF> geometry;

        const auto firstLineIt = Lines.begin() + iLine.first;
        const auto lastLineIt = Lines.begin() + iLine.second;
        const auto startIndex = iChar.first;
        const auto endIndex = iChar.second;

        const QRectF firstLineGeometry = firstLineIt->getGeometryByIndices(startIndex, endIndex);
        geometry.append(firstLineGeometry);

        if (firstLineIt != lastLineIt)
        {
            for (auto lineIt = firstLineIt + 1; lineIt < lastLineIt; ++lineIt)
                geometry.append(lineIt->Geometry);

            const QRectF lastLineGeometry = lastLineIt->getGeometryByIndices(startIndex, endIndex);
            geometry.append(lastLineGeometry);
        }

        return geometry;
    }

    QString getTextByIndices(const int page, const CharIndices& iChar) const
    {
        const auto startIndex = iChar.first;
        const auto endIndex = iChar.second;
        return document->text(page, startIndex, endIndex - startIndex);
    }

    std::optional<DocumentLink> getLink(const int page, const QPointF pos) const
    {
        const auto& layout = getPageLayout(page);

        for (const auto& link : layout.Links)
        {
            for (const auto& rect : link.geometry())
                if (rect.contains(pos))
                    return link;
        }

        return std::nullopt;
    }

private:
    auto getPageLayout(const int page) const -> const PageLayout&
    {
        PageLayout *layout = pageLayoutCache.object(page);

        if (!layout)
        {
            layout = parse(page);
            (void) pageLayoutCache.insert(page, layout); // TODO: make it work calculating layout size after creation
        }

        return *layout;
    }

    auto parse(const int page) const -> PageLayout*
    {
        auto* layout = new PageLayout;

        const QString chars = document->text(page);
        const QList<QRectF> boxes = document->textBoxes(page);
        Q_ASSERT(chars.size() == boxes.size());

        if (boxes.isEmpty())
            return layout;

        LineLayout line;
        line.FirstCharIndex = 0;
        line.Geometry = boxes[0];

        std::optional<double> wordBeginOpt = std::nullopt;

        if (!chars[0].isSpace()) {
            wordBeginOpt = boxes[0].left();
        }

        line.Chars.push_back({ boxes[0].left(), boxes[0].right() });

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

            line.Chars.emplace_back(box.left(), box.right());

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

        layout->Links = document->links(page);

        // NOTE: DEBUG
        DumpPageLayout(*layout, page, *document);

        return layout;
    }

    static auto DumpPageLayout(const PageLayout& layout,
                               const int page,
                               const Document& document) -> void
    {
        for (const auto &[index, geometry, chars, words] : layout.Lines) {
            qDebug() << "    geom: " << geometry;
            qDebug() << "    text: " << document.text(page, index, chars.size());
            qDebug() << "    chars:" << chars;
            qDebug() << "    words:" << words;
        }
    }

    auto getIndices(const int page, const QRectF& rect) const -> std::pair<LineIndices, CharIndices>
    {
      const QRectF region = rect.normalized();

        if (region.isNull())
            return {{ -1, -1 }, { -1, -1 }};

        const PageLayout& layout = getPageLayout(page);
        const auto [firstLineIt, lastLineIt] = layout.findLinesCrossedBy(region);

        if (firstLineIt == layout.Lines.end())
        {
            return {{ -1, -1 }, { -1, -1 }};
        }

        return {
            {
                std::distance(layout.Lines.begin(), firstLineIt),
                std::distance(layout.Lines.begin(), lastLineIt),
            },
            {
                firstLineIt->indices().first + std::distance(firstLineIt->Chars.begin(), firstLineIt->findFirstCharCrossedBy(rect)),
                lastLineIt->indices().first + std::distance(lastLineIt->Chars.begin(), lastLineIt->findLastCharCrossedBy(rect)) + 1,
            }
        };
    }

    auto getLineIndices(const int page, const QPointF& point) const -> std::pair<LineIndices, CharIndices>
    {
        const PageLayout& layout = getPageLayout(page);

        const auto it = std::find_if(layout.Lines.begin(), layout.Lines.end(), [point](const LineLayout& line)
        {
           return line.Geometry.contains(point);
        });

        if (it == layout.Lines.end())
        {
            return {{ -1, -1}, { -1, -1 }};
        }

        return {
            {
                std::distance(layout.Lines.begin(), it),
                std::distance(layout.Lines.begin(), it),
            },
            {
                it->indices().first,
                it->indices().second
            }
        };
    }

    auto getWordIndices(const int page, const QPointF& point) const -> std::pair<LineIndices, CharIndices>
    {
        const PageLayout& layout = getPageLayout(page);

        const auto lineIt = std::find_if(layout.Lines.begin(), layout.Lines.end(), [point](const LineLayout& line)
        {
           return line.Geometry.contains(point);
        });

        if (lineIt == layout.Lines.end())
        {
            return {{ -1, -1}, { -1, -1 }};
        }

        const auto wordBoundariesIt = lineIt->findWordAt(point);

        if (wordBoundariesIt == lineIt->Words.end())
        {
            return {{ -1, -1}, { -1, -1 }};
        }

        const auto [x1, x2] = *wordBoundariesIt;
        const auto [y1, y2] = std::pair(lineIt->Geometry.top(), lineIt->Geometry.bottom());

        const auto rect = QRectF(QPointF { x1, y1 }, QPointF {x2, y2});

        return {
            {
                std::distance(layout.Lines.begin(), lineIt),
                std::distance(layout.Lines.begin(), lineIt),
            },
            { // TODO: simplify (and check)
                lineIt->indices().first + std::distance(lineIt->Chars.begin(), lineIt->findFirstCharCrossedBy(rect)),
                lineIt->indices().first + std::distance(lineIt->Chars.begin(), lineIt->findLastCharCrossedBy(rect)) + 1,
            }
        };
    }

    friend class StandardDocumentParser;

    std::shared_ptr<const Document> document;
    mutable QCache<int, PageLayout> pageLayoutCache;
};

StandardDocumentParser::StandardDocumentParser()
    : d(std::make_unique<Private>())
{
    setLayoutCacheLimit(64 /*MiB*/ * 1024 /*KiB*/ * 1024 /*B*/);
}

StandardDocumentParser::~StandardDocumentParser() = default;

auto StandardDocumentParser::setLayoutCacheLimit(qreal bytes) const -> void
{
    d->pageLayoutCache.setMaxCost(bytes);
}

auto StandardDocumentParser::setDocument(std::shared_ptr<const Document> document) -> void
{
    // Reset active state
    d->pageLayoutCache.clear();

    d->document = document;
}

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

auto StandardDocumentParser::selection() const -> std::unique_ptr<DocumentSelection>
{
    struct TextSelection : DocumentSelection
    {
        explicit TextSelection(Private* const d)
            : d_ptr(d)
        {}

        auto configure(const int page, const Option& option) -> void final
        {
          std::visit(overload {
                [&](const Lines& lines) {
                    m_page = page;
                    std::tie(m_iLine, m_iChar) = d_ptr->getIndices(page, lines.region);
                },
                [&](const Line& line) {
                    m_page = page;
                    std::tie(m_iLine, m_iChar) = d_ptr->getLineIndices(page, line.point);
                },
                [&](const Word& word) {
                    m_page = page;
                    std::tie(m_iLine, m_iChar) = d_ptr->getWordIndices(page, word.point);
                },
            }, option);
        }

        auto empty() const -> bool final
        {
            return m_page == -1 || hash() == (static_cast<int64_t>(-1) << 32 | -1);
        }

        auto hash() const -> uint64_t final
        {
            return static_cast<int64_t>(m_iChar.first) << 32 | m_iChar.second;
        }

        auto text() const -> QString final
        {
            return d_ptr->getTextByIndices(m_page, m_iChar);
        }

        auto geometry() const -> QList<QRectF> final
        {
            return d_ptr->getGeometryByIndices(m_page, m_iLine, m_iChar);
        }

    private:
        int m_page = -1;
        std::pair<int32_t, int32_t> m_iLine;
        std::pair<int32_t, int32_t> m_iChar;
        Private* const d_ptr;
    };

    return std::make_unique<TextSelection>(d.get());
}


auto StandardDocumentParser::hasText(int page, QPointF point) const -> bool
{
    const auto layout = d->getPageLayout(page);
    return layout.findLineAt(point) != layout.Lines.end();
}

auto StandardDocumentParser::hasLink(int page, QPointF point) const -> bool
{
    // TODO: possible optimization
    return d->getLink(page, point).has_value();
}

auto StandardDocumentParser::getLink(int page, QPointF point) const -> std::optional<DocumentLink>
{
    return d->getLink(page, point);
}

