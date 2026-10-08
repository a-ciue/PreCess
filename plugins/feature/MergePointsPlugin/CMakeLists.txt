precess_add_feature_plugin(MergePointsPlugin
    SOURCES "MergePointsHandler.cpp"
    PLUGIN_H "MergePointsPlugin.h"
)
precess_plugin_link_libraries(MergePointsPlugin TKShHealing)

add_subdirectory(test)
