precess_add_feature_plugin(TaskDemoPlugin
	SOURCES "TaskDemoHandler.cpp"
	PLUGIN_H "TaskDemoPlugin.h"
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
