add_library(vtkPart STATIC MeshActor.cpp SelectorHighlight.cpp QRenderWindowStyle.cpp SelectManager.cpp SplineActor.cpp MeshActorManager.cpp SplineActorManager.cpp
)
target_link_libraries(vtkPart PUBLIC Core)
target_link_libraries(vtkPart
    PRIVATE
        Data
        ${OpenCASCADE_LIBRARIES}
)
target_link_libraries(vtkPart
    PRIVATE
    TKernel TKMath TKGeomBase TKGeomAlgo TKTopAlgo TKPrim TKShHealing TKMesh TKMeshVS TKService TKIVtk TKDESTEP
    ${VTK_LIBRARIES}
)
target_include_directories(vtkPart PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
# VTK 模块自动初始化
vtk_module_autoinit(TARGETS vtkPart MODULES ${VTK_LIBRARIES} )
target_link_libraries(vtkPart PRIVATE ${VTK_LIBRARIES})

qt_add_qml_module(renderQml
    URI render
    STATIC
    SOURCES QRenderWindow.h QRenderWindow.cpp
)
target_link_libraries(renderQml PRIVATE 
    vtkPart modelQml 
    ${VTK_LIBRARIES}
)