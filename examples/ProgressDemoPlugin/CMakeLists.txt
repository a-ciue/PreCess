cmake_minimum_required(VERSION 3.27)
# 外部模式声明本目录的插件工程；源码树内模式继承主工程。
if(NOT PRECESS_PLUGIN_IN_TREE)
    project(ProgressDemoPlugin LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../ExampleProject.cmake")
precess_add_algo_plugin(ProgressDemoPlugin
	SOURCES "ProgressDemoHandler.cpp"
	PLUGIN_H "ProgressDemoPlugin.h"
)

if(NOT PRECESS_PLUGIN_IN_TREE)
    precess_plugin_install(ProgressDemoPlugin DESTINATION "${PRECESS_EXAMPLE_PLUGIN_INSTALL_DIR}")
endif()