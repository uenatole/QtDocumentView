#include <QApplication>
#include <QShortcut>
#include <QClipboard>

#include <Document/API/DocumentFacade.h>

#include <DocumentView/DocumentView.h>
#include <DocumentView/DocumentZoomer.h>
#include <DocumentView/DocumentSelector.h>

#include <Document/Pdf/PdfDocument.h>

#include <Document/Ocr/OcrDocument.h>
#include <Document/Ocr/engines/TesseractOcrEngine.h>

#include <Document/Std/StandardDocumentParser.h>
#include <Document/Std/StandardDocumentRenderer.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const auto pdf = std::make_shared<PdfDocument>();
    pdf->load(qEnvironmentVariable("DOCUMENT"));

    const auto ocr = std::make_shared<OcrDocument>();
    ocr->setSource(pdf);
    ocr->setEngine(std::make_unique<TesseractOcrEngine>());

    const auto renderer = std::make_shared<StandardDocumentRenderer>();
    const auto parser = std::make_shared<StandardDocumentParser>();

    const auto document = std::make_shared<DocumentFacade>();
    document->setDocument(pdf);
    document->setRenderer(renderer);
    document->setParser(parser);

    DocumentView view;

    DocumentSelector selector(&view);
    DocumentZoomer zoomer(&view);

    const QShortcut copyShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), &view);
    QObject::connect(&copyShortcut, &QShortcut::activated, [&]
    {
        QGuiApplication::clipboard()->setText(view.getSelectedText(), QClipboard::Clipboard);
    });

    view.setDocument(document);
    view.show();

    return QApplication::exec();
}
