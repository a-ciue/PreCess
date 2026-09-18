# 内嵌停靠组件：全量编译进主程序，不单独分发
add_library(dock STATIC
    core/ClassicIndicators.cpp
    core/Controller.cpp
    core/DockRegistry.cpp
    core/DockWidget.cpp
    core/DragController.cpp
    core/Draggable.cpp
    core/DropArea.cpp
    core/DockTypes.h
    core/DropIndicatorOverlay.cpp
    core/FloatingWindow.cpp
    core/Group.cpp
    core/MainWindow.cpp
    core/WindowBeingDragged.cpp
    engine/Item.cpp
    engine/ItemBoxContainer.cpp
    engine/Separator.cpp
    qtquick/AreaItem.cpp
    qtquick/DockWidgetInstantiator.cpp
    qtquick/DockingAreaView.cpp
    qtquick/FloatingWindowView.cpp
    qtquick/GroupView.cpp
    qtquick/GuestView.cpp
    qtquick/IndicatorsView.cpp
    qtquick/MouseGrabber.cpp
    qtquick/Platform.cpp
    qtquick/QmlTypes.cpp
    qtquick/SeparatorView.cpp
)
target_include_directories(dock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dock PUBLIC
    Qt${VTK_QT_VERSION}::Core
    Qt${VTK_QT_VERSION}::Gui
    Qt${VTK_QT_VERSION}::Quick
)

# 视图 QML 资源（C++ 侧按 qrc:/precess/dock/<文件名> 加载）
set(dock_qml_files
    qtquick/qml/DockGroup.qml
    qtquick/qml/DockSeparator.qml
    qtquick/qml/DockIndicators.qml
    qtquick/qml/DockFloatingWindow.qml
)
foreach(qml_file IN LISTS dock_qml_files)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
qt_add_resources(dock "dock_qml"
    PREFIX "/precess/dock"
    FILES ${dock_qml_files}
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
