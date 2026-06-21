set(TETGEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tetgen")

add_library(TetGenInternal STATIC
    "${TETGEN_SOURCE_DIR}/tetgen.cxx"
    "${TETGEN_SOURCE_DIR}/predicates.cxx"
)

target_compile_definitions(TetGenInternal
    PUBLIC TETLIBRARY
)

target_include_directories(TetGenInternal
    PUBLIC "${TETGEN_SOURCE_DIR}"
)

precess_add_algo_plugin(TetGenLibPlugin
    SOURCES "TetGenLibHandler.cpp"
    PLUGIN_H "TetGenLibPlugin.h"
)

precess_plugin_link_libraries(TetGenLibPlugin TetGenInternal)
