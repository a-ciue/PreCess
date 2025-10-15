precess_add_io_plugin(StepModelPlugin 
	SOURCES "StepModelHandler.cpp" 
	PLUGIN_H "StepModelPlugin.h"
)
precess_plugin_link_libraries(StepModelPlugin TKDESTEP)