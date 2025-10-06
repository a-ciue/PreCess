precess_add_io_plugin(VtkLegacyModelPlugin 
	"VtkLegacyModelHandler.cpp" 
	"VtkLegacyModelPlugin.h"
)
precess_plugin_link_libraries(VtkLegacyModelPlugin VTK::IOLegacy)