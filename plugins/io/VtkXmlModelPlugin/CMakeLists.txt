precess_add_io_plugin(VtkXmlModelPlugin
	SOURCES "VtkXmlModelHandler.cpp"
	PLUGIN_H "VtkXmlModelPlugin.h"
)
precess_plugin_link_libraries(VtkXmlModelPlugin VTK::IOXML)
add_subdirectory(test)
