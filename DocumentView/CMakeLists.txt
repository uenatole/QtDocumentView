option(DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP "Draw page layout markup" ON)

add_library(DocumentView SHARED)

target_sources(DocumentView
    PRIVATE
        src/DocumentPageItem.cpp
        src/DocumentView.cpp

        # NOTE: maybe it should be moved out from DocumentView main target?
        src/DocumentSelector.cpp
        src/DocumentZoomer.cpp
)

target_include_directories(DocumentView
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/DocumentView
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP)
    target_compile_definitions(DocumentView PRIVATE DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

target_link_libraries(DocumentView
    PUBLIC
        Document::API
    PRIVATE
        Qt::Widgets
)
