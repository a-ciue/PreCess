add_library(DataOps STATIC
	ObjMeshIO.cpp
	UGridModel.cpp
	GeometryBuilder.cpp
	GeometryTopologyEditor.cpp
)
target_include_directories(DataOps PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(DataOps PUBLIC Core Data
	VTK::CommonCore
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>	# spdlog日志库
)
target_link_libraries(DataOps PRIVATE
	TKBRep
	TKBool
	TKPrim
	TKTopAlgo
)

# std::execution 并行策略：GCC 的 pstl 后端为 TBB，须显式链接；
# AppleClang 须显式链接 libc++experimental.a
# MSVC 使用自带的 Windows 线程池后端，无需引入
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	find_package(TBB REQUIRED)
	target_link_libraries(DataOps PRIVATE TBB::tbb)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	target_link_libraries(DataOps PRIVATE c++experimental)
endif()

add_subdirectory(test)
