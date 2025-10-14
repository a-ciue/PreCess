find_package(libMeshb)
if (${libMeshb_FOUND})
	precess_add_io_plugin(MeshMeditModelPlugin 
		"MeshMeditModelHandler.cpp" 
		"MeshMeditModelPlugin.h"
		"LibMeshbIO.cpp"
	)
	precess_plugin_link_libraries(MeshMeditModelPlugin Meshb.7)
endif()