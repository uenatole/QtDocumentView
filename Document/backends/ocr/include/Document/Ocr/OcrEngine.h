#pragma once

#include <QFuture>

#include <QString>
#include <QList>
#include <QRectF>

class QImage;

struct OcrEngine
{
    virtual ~OcrEngine() = default;

    struct Result
    {
        QString Chars;
        QList<QRectF> Boxes;
    };

    virtual auto recognize(const QImage& image) const -> QFuture<Result> = 0;
    virtual auto setLanguage(const QString& language) -> void = 0;
};
