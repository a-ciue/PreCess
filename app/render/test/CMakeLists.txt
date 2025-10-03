add_library(TestDeps OBJECT "MakeMeshDataVtk.cpp")
target_include_directories(TestDeps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TestDeps PRIVATE 
    vtkPart
    VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2
)
vtk_module_autoinit(TARGETS TestDeps MODULES VTK::CommonColor VTK::RenderingCore VTK::RenderingOpenGL2)

add_executable(TestSingleEdgeSelectorHighlight TestSingleEdgeSelectorHighlight.cpp)
target_link_libraries(TestSingleEdgeSelectorHighlight PRIVATE 
    vtkPart TestDeps
    VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestSingleEdgeSelectorHighlight MODULES VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::RenderingOpenGL2 VTK::FiltersGeometry)
install(TARGETS TestSingleEdgeSelectorHighlight)

add_executable(TestMeshActor TestMeshActor.cpp)
target_link_libraries(TestMeshActor PRIVATE 
    vtkPart TestDeps
    VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry
)
vtk_module_autoinit(TARGETS TestMeshActor MODULES VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::CommonColor VTK::RenderingCore VTK::RenderingUI VTK::FiltersGeometry)
install(TARGETS TestMeshActor)