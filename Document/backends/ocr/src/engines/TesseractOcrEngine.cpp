#include "engines/TesseractOcrEngine.h"

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <QDebug>
#include <QImage>
#include <QtConcurrent/QtConcurrentRun>
#include <QMutex>

#include "OcrEngine.h"

struct TesseractOcrEngine::Private
{
    // NOTE: thread safety?
    tesseract::TessBaseAPI* tess = nullptr;
    QString currentLanguage = "eng";

    Private()
    {
        tess = new tesseract::TessBaseAPI();
        initializeTesseract();
    }

    ~Private()
    {
        if (tess)
        {
            tess->End();
            delete tess;
        }
    }

    void initializeTesseract() const
    {
        if (!tess)
            return;

        if (tess->Init(nullptr, currentLanguage.toUtf8().constData()))
        {
            qWarning() << "Failed to initialize Tesseract with language:" << currentLanguage;
            return;
        }

        tess->SetPageSegMode(tesseract::PSM_AUTO);
    }

    auto setLanguage(const QString& lang) -> void
    {
        if (currentLanguage == lang)
            return;

        currentLanguage = lang;

        if (tess)
        {
            tess->End();
            initializeTesseract();
        }
    }

    auto recognize(const QImage& imago) const -> Result
    {
        Result result;

        if (!tess || imago.isNull())
            return result;

        QImage image = imago;
        if (image.format() != QImage::Format_RGB888)
        {
            image = image.convertToFormat(QImage::Format_RGB888);
        }

        tess->SetImage(image.bits(), image.width(), image.height(), 3, image.bytesPerLine());
        tess->Recognize(nullptr);

        if (const std::unique_ptr<tesseract::ResultIterator> ri(tess->GetIterator()); ri)
        {
            int x1, y1, x2, y2;
            ri->Begin();

            do {
                if (ri->Empty(tesseract::RIL_SYMBOL))
                    continue;

                if (std::unique_ptr<char[]> symbol(ri->GetUTF8Text(tesseract::RIL_SYMBOL)); symbol)
                {
                    result.Chars += QString::fromUtf8(symbol.get());

                    if (ri->BoundingBox(tesseract::RIL_SYMBOL, &x1, &y1, &x2, &y2))
                    {
                        result.Boxes.append(QRectF(x1, y1, x2 - x1, y2 - y1));
                    }

                    if (ri->IsAtFinalElement(tesseract::RIL_TEXTLINE, tesseract::RIL_SYMBOL))
                    {
                        if (!ri->IsAtFinalElement(tesseract::RIL_PARA, tesseract::RIL_SYMBOL))
                        {
                            result.Chars += '\n';
                            result.Boxes.append(result.Boxes.last().translated(1, 0));
                        }
                    }
                    else if (ri->IsAtFinalElement(tesseract::RIL_WORD, tesseract::RIL_SYMBOL))
                    {
                        result.Chars += ' ';
                        result.Boxes.append(result.Boxes.last().translated(1, 0));
                    }
                }
            }
            while (ri->Next(tesseract::RIL_SYMBOL));
        }

        return result;
    }
};

TesseractOcrEngine::TesseractOcrEngine()
    : d(std::make_unique<Private>())
{}

TesseractOcrEngine::~TesseractOcrEngine() = default;

auto TesseractOcrEngine::recognize(const QImage& image) const -> QFuture<Result>
{
    return QtConcurrent::run([this, image]() -> Result
    {
        return d->recognize(image);
    });
}

auto TesseractOcrEngine::setLanguage(const QString& language) -> void
{
    d->setLanguage(language);
}
