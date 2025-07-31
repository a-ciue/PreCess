add_library(IOSystem STATIC ModelIOSystem.cpp "ModelIOSystemRegister.cpp")
target_link_libraries(IOSystem PRIVATE 
	Data
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)
target_link_libraries(IOSystem PUBLIC Core)
target_include_directories(IOSystem PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)