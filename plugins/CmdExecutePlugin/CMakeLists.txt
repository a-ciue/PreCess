qt_add_plugin(CmdExecutePlugin CLASS_NAME CmdExecutePlugin "CmdExecuteHandler.cpp" "CmdExecutePlugin.h")
target_link_libraries(CmdExecutePlugin PRIVATE
	$<COMPILE_ONLY:Data IOSystem> # 对Data IOSystem只编译时依赖（只在编译时使用头文件），不链接Data.lib IOSystem.lib
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)
target_link_libraries(CmdExecutePlugin PUBLIC Core)