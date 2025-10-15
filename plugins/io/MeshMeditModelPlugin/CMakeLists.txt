find_package(libMeshb)
if (${libMeshb_FOUND})
	precess_add_io_plugin(MeshMeditModelPlugin 
		SOURCES "MeshMeditModelHandler.cpp" "LibMeshbIO.cpp"
		PLUGIN_H "MeshMeditModelPlugin.h"
	)
	precess_plugin_link_libraries(MeshMeditModelPlugin Meshb.7)
	
	add_subdirectory(test)
endif()