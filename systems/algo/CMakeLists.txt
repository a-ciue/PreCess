add_library(AlgorithmSystemQml STATIC)
qt_add_qml_module(AlgorithmSystemQml
    URI systems.algo
    VERSION 1.0
    SOURCES
        QAlgorithmInfo.h QAlgorithmSystemAdaptor.h "QAlgorithmSystemAdaptor.cpp"
)
target_link_libraries(AlgorithmSystemQml PRIVATE 
    Qt6::Core Qt6::Qml spdlog::spdlog)
target_link_libraries(AlgorithmSystemQml PUBLIC QArgObject)
target_link_libraries(AlgorithmSystemQmlplugin PUBLIC QArgObject)

add_library(AlgorithmSystem STATIC AlgorithmSystem.cpp AlgorithmSystemRegister.cpp)
target_link_libraries(AlgorithmSystem PRIVATE 
    Core AlgorithmSystemQmlplugin
    Qt6::Core spdlog::spdlog)
