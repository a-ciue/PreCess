qt_add_qml_module(IOSystemQml
    URI app.model.systems.io
    STATIC
    VERSION 1.0
    SOURCES
        QModelIOInfo.h QModelIOSystemAdaptor.h QModelIOSystemAdaptor.cpp
)
target_link_libraries(IOSystemQml PRIVATE 
    IOSystem
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(IOSystemQml PUBLIC QArgType Core Data)
target_link_libraries(IOSystemQmlplugin PUBLIC QArgType)
target_include_directories(IOSystemQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)