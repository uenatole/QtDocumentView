#pragma once

#include "Document/Ocr/OcrEngine.h"

class TesseractOcrEngine : public OcrEngine
{
public:
    TesseractOcrEngine();
    ~TesseractOcrEngine() override;

    // TODO: cover with unit tests
    auto recognize(const QImage& image) const -> QFuture<Result> final;
    auto setLanguage(const QString& language) -> void final;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
