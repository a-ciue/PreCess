precess_add_io_plugin(VtkLegacyModelPlugin 
	SOURCES "VtkLegacyModelHandler.cpp" 
	PLUGIN_H "VtkLegacyModelPlugin.h"
)
precess_plugin_link_libraries(VtkLegacyModelPlugin VTK::IOLegacy)