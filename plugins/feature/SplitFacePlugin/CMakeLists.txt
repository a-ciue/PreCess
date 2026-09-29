precess_add_feature_plugin(SplitFacePlugin
    SOURCES "SplitFaceHandler.cpp"
    PLUGIN_H "SplitFacePlugin.h"
)
precess_plugin_link_libraries(SplitFacePlugin TKFeat)

add_subdirectory(test)
