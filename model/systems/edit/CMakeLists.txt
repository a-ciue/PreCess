# OBJECT 库：对象经 model/CMakeLists.txt 的 PreCessBase 聚合导出（依赖方向仍由此处强制）
add_library(EditSystem OBJECT EditSystem.cpp EditSystemRegister.cpp)
target_link_libraries(EditSystem PRIVATE 
	Data 
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)
target_link_libraries(EditSystem PUBLIC
    SystemCore Core    
)
target_include_directories(EditSystem PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)

add_subdirectory(test)