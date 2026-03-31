add_library(DocumentSTD SHARED)
add_library(Document::STD ALIAS DocumentSTD)

target_sources(DocumentSTD
    PRIVATE
        src/StandardDocumentParser.cpp

        src/renderer/QCacheExt.h
        src/renderer/RenderCache.cpp
        src/renderer/RenderCache.h
        src/renderer/StandardDocumentRenderer.cpp
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
