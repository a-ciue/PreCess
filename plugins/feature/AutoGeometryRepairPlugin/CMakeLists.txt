precess_add_feature_plugin(AutoGeometryRepairPlugin
    SOURCES "AutoGeometryRepairHandler.cpp"
    PLUGIN_H "AutoGeometryRepairPlugin.h"
)
precess_plugin_link_libraries(AutoGeometryRepairPlugin TKBRep TKShHealing TKTopAlgo)

add_subdirectory(test)
