# OBJECT 库：对象经 model/CMakeLists.txt 的 PreCessBase 聚合为单一静态目标导出
add_library(Core OBJECT "TempFile.cpp" "EventBus.h")
target_include_directories(Core PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
