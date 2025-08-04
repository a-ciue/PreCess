qt_add_plugin(OBJModelPlugin CLASS_NAME OBJModelPlugin "OBJModelHandler.cpp" "OBJModelPlugin.h" "../../CTMeshModel.cpp" "../../ModelUtil.cpp")
target_link_libraries(OBJModelPlugin PRIVATE 
	Core Data
	$<COMPILE_ONLY:IOSystem>
	Qt6::Core
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)