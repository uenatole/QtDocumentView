option(BUILD_PDF_BACKEND "Build WebEngine's (Qt::Pdf) based PDF backend" ON)
option(BUILD_OCR_BACKEND "Build OCR backend (with Tesseract engine)" ON)

add_library(DocumentBackends INTERFACE)
add_library(Document::Backends ALIAS DocumentBackends)

if(BUILD_PDF_BACKEND)
    add_subdirectory(pdf)
    target_link_libraries(DocumentBackends INTERFACE DocumentBackendPdf)
endif()

if(BUILD_OCR_BACKEND)
    add_subdirectory(ocr)
    target_link_libraries(DocumentBackends INTERFACE DocumentBackendOcr)
endif()
