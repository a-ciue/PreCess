precess_add_io_plugin(IgesModelPlugin
	SOURCES
		"IgesModelHandler.cpp"
		"IgesXdeComponentBuilder.cpp"
	PLUGIN_H "IgesModelPlugin.h"
)
precess_plugin_link_libraries(IgesModelPlugin
	TKDEIGES
	TKXCAF
	TKVCAF
	TKCAF
	TKLCAF
)

add_subdirectory(test)
