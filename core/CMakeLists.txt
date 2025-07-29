add_library(Core STATIC "TempFile.cpp")
target_include_directories(Core PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)