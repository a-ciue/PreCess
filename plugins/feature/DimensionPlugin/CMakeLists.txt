precess_add_feature_plugin(DimensionPlugin
	SOURCES "DimensionHandler.cpp"
	PLUGIN_H "DimensionPlugin.h"
)

precess_plugin_link_libraries(DimensionPlugin
	TKernel      # Standard / gp 基础类型
	TKBRep       # BRep_Tool、BRepAdaptor
	TKTopAlgo    # BRepGProp、BRepBndLib
)

add_subdirectory(test)
