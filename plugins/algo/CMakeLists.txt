# wasm 平台不支持命令行执行外部程序
if(NOT EMSCRIPTEN)
add_subdirectory(CmdExecutePlugin)
add_subdirectory(TetGenPlugin)
endif()
add_subdirectory(TetGenLibPlugin)
add_subdirectory(GmshPlugin)
