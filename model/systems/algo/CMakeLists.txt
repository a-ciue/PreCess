add_library(AlgorithmSystem STATIC AlgorithmSystem.cpp AlgorithmSystemRegister.cpp)
target_link_libraries(AlgorithmSystem PRIVATE 
    Core Data 
    $<COMPILE_ONLY:IOSystem> # 对IOSystem只编译时依赖（只在编译时使用头文件），不链接IOSystem.lib
    Qt6::Core spdlog::spdlog)
target_include_directories(AlgorithmSystem PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)