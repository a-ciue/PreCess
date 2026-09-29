precess_add_feature_plugin(PatchFacePlugin
    SOURCES "PatchFaceHandler.cpp"
    PLUGIN_H "PatchFacePlugin.h"
)
precess_plugin_link_libraries(PatchFacePlugin TKShHealing)

add_subdirectory(test)
