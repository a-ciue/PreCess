cmake_minimum_required(VERSION 3.27)
# 外部模式声明本目录的插件工程；源码树内模式继承主工程。
if(NOT PRECESS_PLUGIN_IN_TREE)
    project(ScalePreviewPlugin LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../ExampleProject.cmake")
precess_add_feature_plugin(ScalePreviewPlugin
	SOURCES "ScalePreviewHandler.cpp"
	PLUGIN_H "ScalePreviewPlugin.h"
)
add_subdirectory(test)

if(NOT PRECESS_PLUGIN_IN_TREE)
    precess_plugin_install(ScalePreviewPlugin DESTINATION "${PRECESS_EXAMPLE_PLUGIN_INSTALL_DIR}")
endif()