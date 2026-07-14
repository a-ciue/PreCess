# TetGenLibPlugin CMake 配置
# 通过 find_package 查找预编译的 TetGen 库，构建算法插件 TetGenLibPlugin

find_package(tetgen REQUIRED)

precess_add_algo_plugin(TetGenLibPlugin
    SOURCES "TetGenLibHandler.cpp"
    PLUGIN_H "TetGenLibPlugin.h"
)

precess_plugin_link_libraries(TetGenLibPlugin tetgen::tetgen)

add_subdirectory(test)
