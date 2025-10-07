add_library(TestDeps OBJECT "MakeMeshDataVtk.cpp")
target_include_directories(TestDeps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TestDeps PRIVATE 
    vtkPart
    VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2 VTK::IOXML VTK::IOLegacy
)
vtk_module_autoinit(TARGETS TestDeps MODULES VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2 VTK::IOXML VTK::IOLegacy)

add_executable(TestSingleEdgeSelectorHighlight TestSingleEdgeSelectorHighlight.cpp)
target_link_libraries(TestSingleEdgeSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSingleEdgeSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestSingleEdgeSelectorHighlight)

add_executable(TestSingleSolidSelectorHighlight TestSingleSolidSelectorHighlight.cpp)
target_link_libraries(TestSingleSolidSelectorHighlight PRIVATE 
    vtkPart TestDeps
    spdlog::spdlog
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSingleSolidSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestSingleSolidSelectorHighlight)

add_executable(TestMeshActor TestMeshActor.cpp)
target_link_libraries(TestMeshActor PRIVATE 
    vtkPart TestDeps
    VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestMeshActor MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry)
install(TARGETS TestMeshActor)