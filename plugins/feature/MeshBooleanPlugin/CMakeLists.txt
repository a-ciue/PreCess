precess_add_feature_plugin(MeshBooleanPlugin
    SOURCES "MeshBooleanHandler.cpp"
    PLUGIN_H "MeshBooleanPlugin.h"
)
precess_plugin_link_libraries(MeshBooleanPlugin CgalSupport)

add_subdirectory(test)
