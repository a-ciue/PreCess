add_library(SystemCore INTERFACE)
target_include_directories(SystemCore INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)