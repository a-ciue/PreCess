# 内嵌停靠组件：全量编译进主程序，不单独分发
add_library(dock STATIC
    core/Controller.cpp
    core/DockRegistry.cpp
    core/DockWidget.cpp
    core/DropArea.cpp
    core/FloatingWindow.cpp
    core/Group.cpp
    core/MainWindow.cpp
    engine/Item.cpp
    engine/ItemBoxContainer.cpp
    engine/Separator.cpp
)
target_include_directories(dock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dock PUBLIC Qt${VTK_QT_VERSION}::Core)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
