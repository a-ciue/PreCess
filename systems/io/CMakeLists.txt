add_library(IOSystem STATIC ModelIOSystem.cpp "ModelIOSystemRegister.cpp")
target_link_libraries(IOSystem PRIVATE 
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	${OpenCASCADE_LIBRARIES}
	Qt6::Core
)

qt_add_plugin(OBJModelPlugin CLASS_NAME OBJModelPlugin "OBJModelHandler.cpp" "OBJModelPlugin.h" "../../ModelData.cpp" "../../CTMeshModel.cpp" "../../ModelUtil.cpp")
target_link_libraries(OBJModelPlugin PRIVATE Qt6::Core)