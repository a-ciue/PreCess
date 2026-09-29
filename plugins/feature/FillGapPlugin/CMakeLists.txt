precess_add_feature_plugin(FillGapPlugin
    SOURCES "FillGapHandler.cpp"
    PLUGIN_H "FillGapPlugin.h"
)
precess_plugin_link_libraries(FillGapPlugin TKShHealing)

add_subdirectory(test)
