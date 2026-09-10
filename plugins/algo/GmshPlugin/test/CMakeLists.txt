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

if(NOT EMSCRIPTEN)

add_executable(GmshVtkExample "GmshVtkExample.cpp")
target_link_libraries(GmshVtkExample PRIVATE
    ${GMSH_TARGET}
    Data
    GmshPluginlib
    TKDESTEP
    TKXSBase
    VTK::InteractionStyle
    VTK::CommonColor
    VTK::RenderingCore
    VTK::RenderingUI
    VTK::RenderingOpenGL2
    VTK::FiltersGeometry
)

add_executable(GmshDemo "GmshDemo.cpp")
target_link_libraries(GmshDemo PRIVATE
    ${GMSH_TARGET}
    Data
    GmshPluginlib
    VTK::InteractionStyle
    VTK::CommonColor
    VTK::RenderingCore
    VTK::RenderingUI
    VTK::RenderingOpenGL2
    VTK::FiltersGeometry
)
add_custom_command(TARGET GmshDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${GMSH_TARGET}>
        $<TARGET_FILE_DIR:GmshDemo>
    COMMENT "Copying gmsh.dll to GmshDemo output directory"
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
        $<TARGET_FILE:${GMSH_TARGET}>
        $<TARGET_FILE_DIR:GmshVtkExample>
    COMMENT "Copying gmsh.dll to GmshVtkExample output directory"
)

endif()
