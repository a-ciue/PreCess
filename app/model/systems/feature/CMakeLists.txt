qt_add_qml_module(FeatureSystemQml
    URI app.model.systems.feature
    STATIC
    VERSION 1.0
    SOURCES
        QFeatureInfo.h QFeatureSystemAdaptor.h "QFeatureSystemAdaptor.cpp"
)
target_link_libraries(FeatureSystemQml PRIVATE 
    FeatureSystem
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(FeatureSystemQml PUBLIC QArgType Core Data)
target_link_libraries(FeatureSystemQmlplugin PUBLIC QArgType)
target_include_directories(FeatureSystemQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
