precess_add_feature_plugin(MeasurePlugin
	SOURCES "MeasureHandler.cpp"
	PLUGIN_H "MeasurePlugin.h"
)

precess_plugin_link_libraries(MeasurePlugin
	TKernel      # Standard / gp 基础类型
	TKBRep       # BRep_Tool、BRepAdaptor
	TKTopAlgo    # BRepGProp、BRepBndLib
)

add_subdirectory(test)
