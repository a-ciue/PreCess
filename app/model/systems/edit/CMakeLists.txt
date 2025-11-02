qt_add_qml_module(EditSystemQml
    URI app.model.systems.edit
    STATIC
    VERSION 1.0
    SOURCES
        QEditInfo.h QEditSystemAdaptor.h QEditSystemAdaptor.cpp
)
target_link_libraries(EditSystemQml PRIVATE 
    EditSystem
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(EditSystemQml PUBLIC QArgType Core Data)
target_link_libraries(EditSystemQmlplugin PUBLIC QArgType)
target_include_directories(EditSystemQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)