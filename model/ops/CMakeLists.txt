add_library(DataOps STATIC
	ObjMeshIO.cpp
	CTMeshModel.cpp
)
target_include_directories(DataOps PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(DataOps PUBLIC Core Data
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>	# spdlog日志库
)