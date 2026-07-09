add_library(DocumentBackendOcr SHARED)
add_library(Document::Backends::Ocr ALIAS DocumentBackendOcr)

target_sources(DocumentBackendOcr
    PRIVATE
        src/OcrDocument.cpp

        src/engines/TesseractOcrEngine.cpp # TODO: split into separate target
)

target_include_directories(DocumentBackendOcr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Document/Ocr
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(LEPTONICA REQUIRED IMPORTED_TARGET lept)

target_link_libraries(DocumentBackendOcr
    PUBLIC
        Document::API
    PRIVATE
        PkgConfig::TESSERACT PkgConfig::LEPTONICA
)
