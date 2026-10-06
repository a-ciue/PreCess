# SDK 示例集合：也可用 cmake -S examples/<插件目录> 单独构建。
cmake_minimum_required(VERSION 3.27)
# 外部模式声明本目录的插件工程；源码树内模式继承主工程。
if(NOT PRECESS_PLUGIN_IN_TREE)
    project(PreCessPluginExamples LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ExampleProject.cmake")
add_subdirectory(ExternalPlugin)
add_subdirectory(FeatureDemoPlugin)
add_subdirectory(TaskDemoPlugin)
add_subdirectory(ScalePreviewPlugin)
add_subdirectory(ProgressDemoPlugin)
