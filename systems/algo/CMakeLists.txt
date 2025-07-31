add_library(AlgorithmSystemQml STATIC)
qt_add_qml_module(AlgorithmSystemQml
    URI systems.algo
    VERSION 1.0
    SOURCES
        QAlgorithmInfo.h QAlgorithmSystemAdaptor.h "QAlgorithmSystemAdaptor.cpp"
)
target_link_libraries(AlgorithmSystemQml PRIVATE 
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(AlgorithmSystemQml PUBLIC QArgObject Core Data)
target_link_libraries(AlgorithmSystemQmlplugin PUBLIC QArgObject)

add_library(AlgorithmSystem STATIC AlgorithmSystem.cpp AlgorithmSystemRegister.cpp)
target_link_libraries(AlgorithmSystem PRIVATE 
    Core Data 
    $<COMPILE_ONLY:IOSystem> # 对IOSystem只编译时依赖（只在编译时使用头文件），不链接IOSystem.lib
    Qt6::Core spdlog::spdlog)
target_include_directories(AlgorithmSystem PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)