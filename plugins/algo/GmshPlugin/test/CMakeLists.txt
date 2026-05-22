precess_add_test(GmshPluginTest GmshPluginTest.cpp)
precess_test_link_libraries(GmshPluginTest 
    Data
    Systems
    Core
    TKPrim
    TKBRep
    TKTopAlgo
    TKernel
    TKG3d
    GmshPluginlib
)

# GmshVtkExample
add_executable(GmshVtkExample "GmshVtkExample.cpp")
target_link_libraries(GmshVtkExample PRIVATE
    gmsh::shared
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
    TKBRep       # BRep_Builder, BRepTools
    TKernel      # Standard_* 基础类型
    TKG3d        # gp_* 准则基础
    TKDESTEP     # STEPControl_Reader
    TKXSBase     # STEP/IGES 转换基础
    TKTopAlgo    # TopExp_Explorer
    GmshPluginlib
)
add_custom_command(TARGET GmshVtkExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:gmsh::shared>
        $<TARGET_FILE_DIR:GmshVtkExample>
    COMMENT "Copying gmsh.dll to GmshVtkExample output directory"
)