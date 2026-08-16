precess_add_feature_plugin(MeshRepairPlugin
    SOURCES "MeshRepairHandler.cpp"
    PLUGIN_H "MeshRepairPlugin.h"
)
precess_plugin_link_libraries(MeshRepairPlugin CgalSupport)

add_subdirectory(test)