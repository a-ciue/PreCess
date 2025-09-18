add_library(Data STATIC
	ModelData.cpp
	MeshData.cpp
	SplineData.cpp
	ModelManager.cpp
	ModelOperator.cpp
	ObjMeshIO.cpp
)
target_include_directories(Data PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(Data PUBLIC Core
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>	# spdlog日志库
)
if (OpenCASCADE_FOUND)
	target_link_libraries(Data PRIVATE TKernel)
else()
	# 供插件目标引用Data头文件
	message(WARNING 
	"没有找到 OpenCASCADE 包， Data 目标及其依赖目标与主程序 exe 目标将无法被构建。 当前项目配置仅在链接时不依赖 Data 的构建插件场景有效。\n"
	"OpenCASCADE package not found. Target Data and its dependent targets, including exe target, will not be able to build. Current project configuration only be valid when trying to build plugin without linking to target Data."
	)
endif()