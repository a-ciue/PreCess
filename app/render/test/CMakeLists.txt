add_library(TestDeps OBJECT "MakeMeshDataVtk.cpp")
target_include_directories(TestDeps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TestDeps PRIVATE 
    vtkPart
    VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2 VTK::IOXML VTK::IOLegacy
)
target_link_libraries(TestDeps PUBLIC DataTest)
vtk_module_autoinit(TARGETS TestDeps MODULES VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2 VTK::IOXML VTK::IOLegacy)

# TestEdgeSelectorHighlight
add_executable(TestEdgeSelectorHighlight TestEdgeSelectorHighlight.cpp)
target_link_libraries(TestEdgeSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestEdgeSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

# TestFaceSelectorHighlight
add_executable(TestFaceSelectorHighlight "TestFaceSelectorHighlight.cpp")
target_link_libraries(TestFaceSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestFaceSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

# TestSolidSelectorHighlight
add_executable(TestSolidSelectorHighlight "TestSolidSelectorHighlight.cpp")
target_link_libraries(TestSolidSelectorHighlight PRIVATE 
    vtkPart TestDeps
    spdlog::spdlog
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSolidSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

# TestVertexSelectorHighlight
add_executable(TestVertexSelectorHighlight TestVertexSelectorHighlight.cpp)
target_link_libraries(TestVertexSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestVertexSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

# TestMeshActor
add_executable(TestMeshActor TestMeshActor.cpp)
target_link_libraries(TestMeshActor PRIVATE 
    vtkPart TestDeps
    VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestMeshActor MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry)

# TestMeshData
add_executable(TestMeshData TestMeshData.cpp)
target_link_libraries(TestMeshData PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestVertexSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

# TestCadShapePicker (OCCT IVtkTools_ShapePicker)
add_executable(TestCadShapePicker "TestGeometryShapePicker.cpp")
target_link_libraries(TestCadShapePicker PRIVATE
    vtkPart TestDeps
    spdlog::spdlog

    VTK::InteractionStyle VTK::CommonColor
    VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2

    TKIVtk
    TKPrim TKBRep TKGeomBase TKMath TKernel TKTopAlgo
    TKDESTEP
    TKXCAF
    TKVCAF
    TKCAF
    TKLCAF
)
vtk_module_autoinit(TARGETS TestCadShapePicker MODULES
    VTK::InteractionStyle VTK::CommonColor
    VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2
)
install(TARGETS TestCadShapePicker)

# TestInteractionService（通用视口交互服务独立测试，FakeInteraction 替代真实插件）
add_executable(TestInteractionService TestInteractionService.cpp)
target_link_libraries(TestInteractionService PRIVATE
    vtkPart TestDeps
    spdlog::spdlog
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry

    TKIVtk
    TKPrim TKBRep TKGeomBase TKMath TKernel TKTopAlgo
)
vtk_module_autoinit(TARGETS TestInteractionService MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)

precess_add_test(TestMeshTopologyDiagnostics TestMeshTopologyDiagnostics.cpp)
precess_test_link_libraries(TestMeshTopologyDiagnostics vtkPart TestDeps)

# TestSelectorHighlightDeselect（回归：取消最后一个选中后，partition 高亮必须清空而不是残留）
precess_add_test(TestSelectorHighlightDeselect TestSelectorHighlightDeselect.cpp)
precess_test_link_libraries(TestSelectorHighlightDeselect vtkPart TestDeps spdlog::spdlog
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSelectorHighlightDeselect MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
