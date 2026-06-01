set_source_files_properties(App.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(coreQml 
    URI app.core 
    STATIC 
    SOURCES QSelection.h
    QML_FILES App.qml
)
target_include_directories(coreQml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(coreQml PUBLIC Core)
target_link_libraries(coreQml PRIVATE Qt${VTK_QT_VERSION}::Qml)