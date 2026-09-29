precess_add_feature_plugin(StitchGeometryPlugin
    SOURCES "StitchGeometryHandler.cpp"
    PLUGIN_H "StitchGeometryPlugin.h"
)
precess_plugin_link_libraries(StitchGeometryPlugin TKShHealing)

add_subdirectory(test)
