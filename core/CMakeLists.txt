add_library(Core STATIC "TempFile.cpp" "EventBus.h")
target_include_directories(Core PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)