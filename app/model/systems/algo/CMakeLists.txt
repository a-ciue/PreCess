add_library(AlgorithmSystemQml STATIC)
qt_add_qml_module(AlgorithmSystemQml
    URI systems.algo
    VERSION 1.0
    SOURCES
        QAlgorithmInfo.h QAlgorithmSystemAdaptor.h "QAlgorithmSystemAdaptor.cpp"
)
target_link_libraries(AlgorithmSystemQml PRIVATE 
    AlgorithmSystem
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(AlgorithmSystemQml PUBLIC QArgType Core Data)
target_link_libraries(AlgorithmSystemQmlplugin PUBLIC QArgType)
target_include_directories(AlgorithmSystemQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)