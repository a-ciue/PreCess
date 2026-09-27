add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(algo)
add_subdirectory(edit)
add_subdirectory(feature)

# OBJECT 库：对象经 model/CMakeLists.txt 的 PreCessBase 聚合导出（依赖方向仍由此处强制）
add_library(Systems OBJECT "SystemPluginManager.cpp")
target_include_directories(Systems PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(Systems PRIVATE 
	SystemCore
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)
target_link_libraries(Systems INTERFACE IOSystem AlgorithmSystem EditSystem FeatureSystem)