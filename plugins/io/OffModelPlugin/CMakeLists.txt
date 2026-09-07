precess_add_io_plugin(OffModelPlugin 
	SOURCES "OffModelHandler.cpp" "OffMeshIO.cpp"
	PLUGIN_H "OffModelPlugin.h"
)

add_subdirectory(test)
