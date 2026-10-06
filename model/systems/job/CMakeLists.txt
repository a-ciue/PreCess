# OBJECT 库：对象经 model/CMakeLists.txt 的 PreCessBase 聚合导出（依赖方向仍由此处强制）
add_library(Job OBJECT JobRunner.cpp ShadowComponent.cpp)
target_link_libraries(Job PRIVATE
	Data
	spdlog::spdlog
)
target_include_directories(Job PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)
