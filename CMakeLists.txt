cmake_minimum_required(VERSION 4.1)
project(MyPdfView)

set(CMAKE_CXX_STANDARD 20)

# TODO: get rid of patched Qt::Pdf and use FPDFium directly
# NOTE: is not needed when BUILD_PDF_BACKEND is OFF, but, unfortunately, will be included anyway.
set(FEATURE_qtwebengine_build OFF CACHE BOOL "" FORCE)
set(QT_GENERATE_SBOM OFF CACHE BOOL "" FORCE)
set(QT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/qtwebengine)

add_subdirectory(Document)
add_subdirectory(DocumentView)

# TODO: move it out
# <
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(example examples/main.cpp)

target_link_libraries(example
        PRIVATE
            DocumentView
            Document::STD
            Document::Backends::Pdf
            Document::Backends::Ocr
            Qt::Widgets
)
# >
