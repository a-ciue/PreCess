add_library(IOSystem STATIC ModelIOSystem.cpp "ModelIOSystemRegister.cpp")
target_link_libraries(IOSystem PRIVATE 
	Data
	spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
	Qt6::Core
)
target_link_libraries(IOSystem PUBLIC Core)

qt_add_plugin(OBJModelPlugin CLASS_NAME OBJModelPlugin "OBJModelHandler.cpp" "OBJModelPlugin.h" "../../data/ModelData.cpp" "../../CTMeshModel.cpp" "../../ModelUtil.cpp")
target_link_libraries(OBJModelPlugin PRIVATE Qt6::Core)