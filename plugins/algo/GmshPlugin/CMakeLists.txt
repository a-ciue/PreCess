find_package(gmsh REQUIRED)

set(GMSH_SOURCE_DIR "" CACHE PATH "Gmsh source tree for internal API")
set(GMSH_BUILD_DIR "" CACHE PATH "Gmsh build tree for generated internal headers")

if(NOT GMSH_SOURCE_DIR OR NOT GMSH_BUILD_DIR)
    message(FATAL_ERROR
        "GmshPlugin requires GMSH_SOURCE_DIR and GMSH_BUILD_DIR")
endif()

precess_add_algo_plugin(GmshPlugin
    SOURCES
        "GmshMeshHandler.cpp"
        "GmshIncrementalMeshState.cpp"
        "IncrementalMeshContext.cpp"
        "IncrementalMeshTools.cpp"
    PLUGIN_H "GmshMeshPlugin.h"
)

target_include_directories(GmshPluginlib PRIVATE
    "${GMSH_BUILD_DIR}/src/common"
    "${GMSH_SOURCE_DIR}/src/common"
    "${GMSH_SOURCE_DIR}/src/geo"
    "${GMSH_SOURCE_DIR}/src/mesh"
    "${GMSH_SOURCE_DIR}/src/numeric"
)

precess_plugin_link_libraries(GmshPlugin 
    gmsh::lib
    TKBRep       # BRep_Builder, BRepTools
    TKernel      # Standard_* 基础类型
    TKG3d        # gp_* 准则基础
    TKDESTEP     # STEPControl_Reader
    TKXSBase     # STEP/IGES 转换基础
    TKTopAlgo    # TopExp_Explorer
    TKG2d
    TKDEIGES
    TKOffset
    TKFeat
    TKFillet
    TKBool
    TKMesh
    TKHLR
    TKBO
    TKPrim
    TKShHealing
    TKGeomAlgo
    TKMath
)

target_link_libraries(GmshPluginlib PUBLIC winmm wsock32 ws2_32 psapi)

add_subdirectory(test)
