precess_add_feature_plugin(SplitEdgePlugin SOURCES "SplitEdgeHandler.cpp" PLUGIN_H "SplitEdgePlugin.h")
precess_plugin_link_libraries(SplitEdgePlugin TKShHealing)
add_subdirectory(test)
