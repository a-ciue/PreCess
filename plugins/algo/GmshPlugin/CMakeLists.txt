find_package(gmsh REQUIRED)

precess_add_algo_plugin(GmshPlugin
    SOURCES
        "GmshMeshHandler.cpp"
        "IncrementalMeshTools.cpp"
    PLUGIN_H "GmshMeshPlugin.h"
)
precess_plugin_link_libraries(GmshPlugin 
    gmsh::shared
    TKBRep       # BRep_Builder, BRepTools
    TKernel      # Standard_* 基础类型
    TKG3d        # gp_* 准则基础
    TKDESTEP     # STEPControl_Reader
    TKXSBase     # STEP/IGES 转换基础
    TKTopAlgo    # TopExp_Explorer
)


add_subdirectory(test)