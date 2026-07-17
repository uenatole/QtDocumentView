add_library(DocumentSTD SHARED)
add_library(Document::STD ALIAS DocumentSTD)

target_sources(DocumentSTD
    PRIVATE
        src/renderer/QCacheExt.h
        src/renderer/RenderCache.cpp
        src/renderer/RenderCache.h
        src/renderer/StandardDocumentRenderer.cpp

        src/parser/layout/DocumentLayout.cpp
        src/parser/layout/DocumentLayout.h
        src/parser/layout/PageLayout.cpp
        src/parser/layout/PageLayout.h
        src/parser/layout/LineLayout.cpp
        src/parser/layout/LineLayout.h
        src/parser/StandardDocumentParsing.cpp
        src/parser/StandardPageParsing.cpp
)

target_include_directories(DocumentSTD
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Document/Std
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(DocumentSTD
    PUBLIC
        Document::API
)
