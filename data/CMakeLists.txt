add_library(Data STATIC
	ModelData.cpp
	MeshData.cpp
	SplineData.cpp
	ModelManager.cpp
	ModelOperator.cpp
)
target_include_directories(Data
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 项目根目录
)
target_link_libraries(Data PUBLIC Core
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)
target_link_libraries(Data PRIVATE 
	TKernel
)