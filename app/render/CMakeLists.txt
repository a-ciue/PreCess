add_library(vtkPart STATIC 
    MeshActor.cpp
    MeshActorSelectOp.cpp
    SelectorHighlight.cpp
    QRenderWindowStyle.cpp
    SelectManager.cpp
    GeometryActor.cpp
    MeshActorManager.cpp
    GeometryActorManager.cpp
    SolidSelectorHighlight.cpp
    VertexSelectorHighlight.cpp
    EdgeSelectorHighlight.cpp
    FaceSelectorHighlight.cpp
    renderStrategy/AttributeOperator.cpp
    renderStrategy/AttriRenderStrategyScalar.cpp
    renderStrategy/AttriRenderStrategyVector.cpp
    renderStrategy/AttriRenderStrategyRGB.cpp
    renderStrategy/AttriRenderStrategyUV.cpp
    renderStrategy/IAttributeRenderStrategy.cpp
)
target_include_directories(vtkPart PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(vtkPart PUBLIC Core)
target_link_libraries(vtkPart
    PRIVATE
        Data
        TKIVtk TKDESTEP
        VTK::InteractionStyle VTK::FiltersGeometry VTK::FiltersExtraction
        VTK::FiltersSources
        VTK::FiltersTexture
)
vtk_module_autoinit(TARGETS vtkPart MODULES VTK::InteractionStyle VTK::FiltersGeometry VTK::FiltersExtraction VTK::FiltersSources VTK::FiltersTexture)

qt_add_qml_module(renderQml
    URI app.render
    STATIC
    SOURCES QRenderWindow.h QRenderWindow.cpp
)
target_link_libraries(renderQml PRIVATE 
    vtkPart modelQml 
    VTK::GUISupportQtQuick VTK::CommonColor
)
vtk_module_autoinit(TARGETS renderQml MODULES 
    VTK::GUISupportQtQuick VTK::CommonColor
)
add_subdirectory(test)