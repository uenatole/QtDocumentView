#pragma once

#include <Document/API/Document.h>

struct OcrEngine;

class OcrDocument : public Document
{
public:
    OcrDocument();
    ~OcrDocument() override;

    void setSource(const std::shared_ptr<const Document>& source);
    void setEngine(std::unique_ptr<OcrEngine>&& engine);

    auto pageCount() const -> std::size_t final;
    auto pagePointSize(int page) const -> QSizeF final;

    auto textReady(int page) const -> bool final;
    auto forceTextReadiness(int page) const -> QFuture<void> final;

    auto text(int page, int from, int count) const -> QString final;
    auto textBoxes(int page, int from, int count) const -> QList<QRectF> final;

    auto render(int page, qreal scale) const -> QFuture<QImage> final;
    auto links(int page) const -> QList<DocumentLink> final;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
