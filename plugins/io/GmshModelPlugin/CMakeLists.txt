precess_add_io_plugin(GmshModelPlugin
	SOURCES "GmshModelHandler.cpp"
	PLUGIN_H "GmshModelPlugin.h"
)

add_subdirectory(test)
