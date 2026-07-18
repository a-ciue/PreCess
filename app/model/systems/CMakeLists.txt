add_subdirectory(algo)
add_subdirectory(io)
add_subdirectory(edit)
add_subdirectory(feature)

qt_add_qml_module(SystemQml
    URI app.model.systems
    STATIC
    VERSION 1.0
    SOURCES
        QSystemPluginManager.h QSystemPluginManager.cpp
)
target_link_libraries(SystemQml PRIVATE 
    Systems
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(SystemQml PUBLIC AlgorithmSystemQml IOSystemQml EditSystemQml FeatureSystemQml)
target_link_libraries(SystemQmlplugin PUBLIC AlgorithmSystemQmlplugin IOSystemQmlplugin EditSystemQmlplugin FeatureSystemQmlplugin)
target_include_directories(SystemQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
