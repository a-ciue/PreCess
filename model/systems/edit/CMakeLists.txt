add_library(EditSystem STATIC EditSystem.cpp EditSystemRegister.cpp)
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