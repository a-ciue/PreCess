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
install(TARGETS TestEdgeSelectorHighlight)

# TestFaceSelectorHighlight
add_executable(TestFaceSelectorHighlight "TestFaceSelectorHighlight.cpp")
target_link_libraries(TestFaceSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestFaceSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestFaceSelectorHighlight)

# TestSolidSelectorHighlight
add_executable(TestSolidSelectorHighlight "TestSolidSelectorHighlight.cpp")
target_link_libraries(TestSolidSelectorHighlight PRIVATE 
    vtkPart TestDeps
    spdlog::spdlog
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSolidSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestSolidSelectorHighlight)

# TestVertexSelectorHighlight
add_executable(TestVertexSelectorHighlight TestVertexSelectorHighlight.cpp)
target_link_libraries(TestVertexSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestVertexSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestVertexSelectorHighlight)

# TestMeshActor
add_executable(TestMeshActor TestMeshActor.cpp)
target_link_libraries(TestMeshActor PRIVATE 
    vtkPart TestDeps
    VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestMeshActor MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry)
install(TARGETS TestMeshActor)