# 外部插件独立构建示例：不依赖 PreCess 源码树，仅 find_package(PreCess)
# 构建：cmake -B build -S . -DCMAKE_PREFIX_PATH=<PreCess 安装前缀>
#       cmake --build build
#       cmake --install build   # 装入 PreCess 插件目录，启动即被发现
cmake_minimum_required(VERSION 3.24)
project(ExternalDemoPlugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PreCess 0.4 REQUIRED)
# 插件自用的第三方依赖示例（同 PreCess 构建时的同一 VTK 实例上追加组件）：
# find_package(VTK REQUIRED COMPONENTS FiltersGeneral)

precess_add_feature_plugin(ExternalDemoPlugin
    SOURCES "ExternalDemoHandler.cpp"
    PLUGIN_H "ExternalDemoPlugin.h"
)
# precess_plugin_link_libraries(ExternalDemoPlugin VTK::FiltersGeneral)
precess_plugin_install(ExternalDemoPlugin)
