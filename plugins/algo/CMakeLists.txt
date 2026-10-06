# wasm 平台不支持命令行执行外部程序
if(NOT EMSCRIPTEN)
add_subdirectory(CmdExecutePlugin)
endif()
add_subdirectory(ProgressDemoPlugin)
