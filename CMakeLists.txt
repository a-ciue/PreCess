cmake_minimum_required(VERSION 3.17)

set(MYNAME PreCess)

project(${MYNAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


option(ENABLE_TEST "是否启用测试")

find_package(spdlog REQUIRED)

find_package(OpenCASCADE REQUIRED)

find_package(VTK REQUIRED COMPONENTS
        CommonColor
        CommonCore
        GUISupportQtQuick
        InteractionStyle
)

find_package("Qt${VTK_QT_VERSION}" REQUIRED COMPONENTS Gui Quick)
message(STATUS "QT_VERSION: Qt${QT_VERSION_MAJOR}")
qt_policy(SET QTP0001 NEW)
qt_standard_project_setup()

add_subdirectory(core)
add_subdirectory(model)
add_subdirectory(plugins)

add_subdirectory(app)

if(ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()