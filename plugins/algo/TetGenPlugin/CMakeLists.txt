precess_add_algo_plugin(TetGenPlugin 
	SOURCES "TetGenHandler.cpp" 
	PLUGIN_H "TetGenPlugin.h"
)

# 尝试从默认路径找到 tetgen 可执行文件
find_program(TETGEN_EXECUTABLE
    NAMES tetgen
    DOC "Path to tetgen executable"
)
if(TETGEN_EXECUTABLE)
    message(STATUS "Found tetgen executable: ${TETGEN_EXECUTABLE}")
    add_custom_command(
        TARGET TetGenPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TETGEN_EXECUTABLE}
            ${CMAKE_BINARY_DIR}
        COMMENT "Copying ${TETGEN_EXECUTABLE} to build directory"
    )
    install(PROGRAMS ${TETGEN_EXECUTABLE}
        DESTINATION bin
        COMPONENT AllPlugins
    )
else()
    message(WARNING "tetgen executable not found. Please set TETGEN_EXECUTABLE to the path of the tetgen executable.")
endif()
