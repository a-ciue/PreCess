precess_add_feature_plugin(CollapseEdgePlugin SOURCES "CollapseEdgeHandler.cpp" PLUGIN_H "CollapseEdgePlugin.h")
precess_plugin_link_libraries(CollapseEdgePlugin TKShHealing)
add_subdirectory(test)
