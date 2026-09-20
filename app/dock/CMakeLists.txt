# 内嵌停靠组件：全量编译进主程序，不单独分发
add_library(dock STATIC
    docking/ZoneGeometry.cpp
    docking/DockObject.cpp
    docking/DockCatalog.cpp
    docking/DockPanel.cpp
    docking/DragSession.cpp
    docking/DragHandle.cpp
    docking/DockRegion.cpp
    docking/DockEnums.h
    docking/ZoneResolver.cpp
    docking/DockWindow.cpp
    docking/PanelGroup.cpp
    docking/DockHost.cpp
    docking/DragProxy.cpp
    docking/DropResolver.cpp
    docking/DropTarget.h
    tree/LayoutNode.cpp
    tree/BoxNode.cpp
    tree/Divider.cpp
    ui/DockAreaItem.cpp
    ui/DockPanelItem.cpp
    ui/DockHostItem.cpp
    ui/DockWindowItem.cpp
    ui/PanelGroupItem.cpp
    ui/PanelContentView.cpp
    ui/DropZoneOverlay.cpp
    ui/GlobalMouseRelay.cpp
    ui/DockRuntime.cpp
    ui/QmlRegistration.cpp
    ui/DividerItem.cpp
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
    ui/qml/PanelGroup.qml
    ui/qml/Divider.qml
    ui/qml/DropZones.qml
    ui/qml/DockWindow.qml
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
