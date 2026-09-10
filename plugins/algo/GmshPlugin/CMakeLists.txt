find_package(gmsh REQUIRED)

if(EMSCRIPTEN)
    set(GMSH_TARGET gmsh::lib)
else()
    set(GMSH_TARGET gmsh::shared)
endif()

precess_add_algo_plugin(GmshPlugin
    SOURCES
        "GmshMeshHandler.cpp"
        "IncrementalMeshTools.cpp"
    PLUGIN_H "GmshMeshPlugin.h"
)

precess_plugin_link_libraries(GmshPlugin
    ${GMSH_TARGET}
    TKBRep       # BRep_Builder, BRepTools
    TKernel      # Standard_* 基础类型
    TKG3d        # gp_* 准则基础
    TKTopAlgo    # TopExp_Explorer
)

add_subdirectory(test)
