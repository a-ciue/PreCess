# 外部插件独立构建示例：不依赖 PreCess 源码树，仅 find_package(PreCess)。
# 使用过程（第三方依赖准备、两行命令、安装位置）见本目录 README.md。
#
# == SDK 插件构建接口（find_package(PreCess) 之后即可直接使用）==
#
# precess_add_io_plugin / precess_add_algo_plugin /
# precess_add_edit_plugin / precess_add_feature_plugin
#     precess_add_<系统>_plugin(<目标名>
#         SOURCES <实现源文件...>   插件实现（Handler 及其配套源）
#         PLUGIN_H <插件头.h>       含 Q_PLUGIN_METADATA 的插件头，
#                                  元数据 JSON 随之内嵌进 DLL
#         [STATIC]                  仅源码树内静态插件使用；外部工程不适用
#     )
#     四个函数对应 PreCess 的四类插件系统（模型 IO / 算法 / 编辑 / 功能），
#     目标名与插件 JSON 中的名称保持一致。声明后即得到模块 DLL 目标，
#     并已链接 PreCess::Base（含各系统接口与其传递依赖）。
#
# precess_plugin_link_libraries(<目标> <库>...)
#     追加插件私有的三方链接依赖（如特定 VTK 组件）：链接进插件，
#     安装时一并收拢其运行时 DLL。使用前先 find_package 到对应目标。
#
# precess_plugin_install(<目标> [DESTINATION <目录>])
#     安装插件：默认装入 PreCess 运行时扫描的 plugins/（相对 --prefix 解析），
#     并收拢运行时三方依赖；DESTINATION 可改指其他目录。
#
# 构建（在本目录执行）：
#   cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=<SDK 前缀>;PreCess-deps 根" -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
#   cmake --install build --prefix <PreCess 安装目录>
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
