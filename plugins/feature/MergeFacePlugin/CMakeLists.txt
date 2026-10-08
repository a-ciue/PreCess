precess_add_feature_plugin(MergeFacePlugin
    SOURCES "MergeFaceHandler.cpp"
    PLUGIN_H "MergeFacePlugin.h"
)
precess_plugin_link_libraries(MergeFacePlugin TKShHealing)

add_subdirectory(test)
