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
        CharIndices Indices; // [a; b)
        QRectF Geometry;
        QList<InLineRange> Chars;
        QList<InLineRange> Words;

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
            if (begin >= Indices.second || begin >= end)
                return {};

            begin = std::max(Indices.first, begin);
            end = std::min(Indices.second, end);

            if (begin >= end)
                return {};

            const qreal left = Chars[begin - Indices.first].first;
            const qreal right = (end == Indices.second)
                ? Geometry.right()
                : Chars[end - Indices.first].first;

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

    QString getText(const int page, const CharIndices& iChar) const
    {
        const auto startIndex = iChar.first;
        const auto endIndex = iChar.second;
        return document->text(page, startIndex, endIndex - startIndex + 1);
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
        PageLayout* layout = pageLayoutCache.object(page);

        if (!layout)
        {
            layout = new PageLayout();

            QString chars = document->text(page); // NOTE: for word bounds (redundant?; this can be done using geometry with reasonable spacing between printable characters) TODO: try it
            QList<QRectF> charBoxes = document->textBoxes(page);

            // Line forming method
            {
                QList<LineLayout> lines;

                // TODO: change line detection method because right now it sometimes is wrong
                const auto isOnLine = [](const QRectF& charRect, const QRectF& lineRect) -> bool
                {
                    return charRect.top() <= lineRect.bottom() && charRect.bottom() >= lineRect.top();
                };

                LineLayout currentLine = {};
                int startIndex = 0;

                QList<InLineRange> words;
                std::optional<qreal> wordBegin = std::nullopt;

                for (int i = 0; i < charBoxes.size(); ++i)
                {
                    const auto& c = chars[i];
                    const auto& box = charBoxes[i];

                    if (currentLine.Chars.isEmpty())
                    {
                        currentLine.Geometry = box;
                        currentLine.Chars.emplaceBack(box.left(), box.right());
                        startIndex = i;
                    }
                    else
                    {
                        if (isOnLine(currentLine.Geometry, box))
                        {
                            currentLine.Geometry |= box; // TODO: optimize calculations
                            currentLine.Chars.emplaceBack(box.left(), box.right());
                        }
                        else
                        {
                            currentLine.Indices = qMakePair(startIndex, i);
                            currentLine.Words = words;
                            lines.append(currentLine);

                            currentLine = {};

                            words.clear();
                            wordBegin = std::nullopt;

                            currentLine.Geometry = box;
                            currentLine.Chars.emplaceBack(box.left(), box.right());
                            startIndex = i;
                        }
                    }

                    if (!c.isSpace() && !wordBegin)
                    {
                        wordBegin = box.left();
                    }
                    else if (c.isSpace() && wordBegin)
                    {
                        words.push_back({ *wordBegin, charBoxes[i - 1].right() });
                        wordBegin = std::nullopt;
                    }
                }

                if (!currentLine.Chars.isEmpty())
                {
                    if (wordBegin)
                    {
                        words.push_back({ *wordBegin, charBoxes.last().right() });
                    }

                    currentLine.Indices = qMakePair(startIndex, charBoxes.size());
                    currentLine.Words = words;
                    lines.append(currentLine);
                }

                layout->Lines = lines;
            }

            layout->Links = document->links(page);

            (void) pageLayoutCache.insert(page, layout); // TODO: make it work calculating layout size after creation

            qDebug() << "Page:" << page;
            for (const auto& line : layout->Lines) {
                qDebug() << "    " << line.Indices << line.Geometry << line.Geometry.top();
                qDebug() << "    " << line.Words;
            }

            for (const auto& link : layout->Links)
                qDebug().noquote() << "    " << link.toString();
        }

        return *layout;
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
                firstLineIt->Indices.first + std::distance(firstLineIt->Chars.begin(), firstLineIt->findFirstCharCrossedBy(rect)),
                lastLineIt->Indices.first + std::distance(lastLineIt->Chars.begin(), lastLineIt->findLastCharCrossedBy(rect)) + 1,
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
                it->Indices.first,
                it->Indices.second
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
                lineIt->Indices.first + std::distance(lineIt->Chars.begin(), lineIt->findFirstCharCrossedBy(rect)),
                lineIt->Indices.first + std::distance(lineIt->Chars.begin(), lineIt->findLastCharCrossedBy(rect)) + 1,
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
            return d_ptr->getText(m_page, m_iChar);
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

