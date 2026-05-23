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

add_executable(GmshVtkExample "GmshVtkExample.cpp")
target_link_libraries(GmshVtkExample PRIVATE
    gmsh::shared
    Data
    GmshPluginlib
    VTK::InteractionStyle
    VTK::CommonColor
    VTK::RenderingCore
    VTK::RenderingUI
    VTK::RenderingOpenGL2
    VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS GmshVtkExample MODULES
    VTK::InteractionStyle
    VTK::CommonColor
    VTK::RenderingCore
    VTK::RenderingUI
    VTK::RenderingOpenGL2
    VTK::FiltersGeometry
)
add_custom_command(TARGET GmshVtkExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:gmsh::shared>
        $<TARGET_FILE_DIR:GmshVtkExample>
    COMMENT "Copying gmsh.dll to GmshVtkExample output directory"
)
