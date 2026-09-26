precess_add_io_plugin(VtkXmlModelPlugin
	SOURCES "VtkXmlModelHandler.cpp"
	SOURCES "VtkXmlReader.cpp"
	PLUGIN_H "VtkXmlModelPlugin.h"
)

add_subdirectory(test)
