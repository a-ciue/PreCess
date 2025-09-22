precess_add_io_plugin(StepModelPlugin 
	"StepModelHandler.cpp" 
	"StepModelPlugin.h"
)
target_link_libraries(StepModelPlugin PRIVATE TKDESTEP)