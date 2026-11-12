precess_add_feature_plugin(MeshQualityPlugin
    SOURCES "MeshQualityHandler.cpp" "SolidCellRecognition.cpp"
    PLUGIN_H "MeshQualityPlugin.h"
)
precess_plugin_link_libraries(MeshQualityPlugin VTK::FiltersVerdict VTK::verdict)

add_subdirectory(test)
