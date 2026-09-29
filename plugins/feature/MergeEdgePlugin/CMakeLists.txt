precess_add_feature_plugin(MergeEdgePlugin
    SOURCES "MergeEdgeHandler.cpp"
    PLUGIN_H "MergeEdgePlugin.h"
)
precess_plugin_link_libraries(MergeEdgePlugin TKShHealing)

add_subdirectory(test)
