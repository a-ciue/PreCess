# TetGenLibPlugin CMake 配置
# 将 TetGen 源码编译为内部静态库 TetGenInternal，并构建算法插件 TetGenLibPlugin

set(TETGEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tetgen")

add_library(TetGenInternal STATIC
    "${TETGEN_SOURCE_DIR}/tetgen.cxx"
    "${TETGEN_SOURCE_DIR}/predicates.cxx"
)

target_compile_definitions(TetGenInternal
    PUBLIC TETLIBRARY
)

# 抑制第三方库的编译器警告，避免污染主项目构建日志
if(MSVC)
    target_compile_options(TetGenInternal PRIVATE /w)
else()
    target_compile_options(TetGenInternal PRIVATE -w)
endif()

target_include_directories(TetGenInternal
    PUBLIC "${TETGEN_SOURCE_DIR}"
)

precess_add_algo_plugin(TetGenLibPlugin
    SOURCES "TetGenLibHandler.cpp"
    PLUGIN_H "TetGenLibPlugin.h"
)

precess_plugin_link_libraries(TetGenLibPlugin TetGenInternal)

add_subdirectory(test)
