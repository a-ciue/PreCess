add_library(vtkPart STATIC 
    MeshActor.cpp
    MeshActorSelectOp.cpp
    GeometryActorSelectOp.cpp
    GeometrySelectorHighlight.cpp
    QRenderWindowStyle.cpp
    SelectManager.cpp
    GeometrySelectManager.cpp
    MeshSelectManager.cpp
    GeometryActor.cpp
    MeshActorManager.cpp
    GeometryActorManager.cpp
    SolidSelectorHighlight.cpp
    VertexSelectorHighlight.cpp
    EdgeSelectorHighlight.cpp
    FaceSelectorHighlight.cpp
    ComponentSelectorHighlight.cpp
    MeshActorManagerSelectOp.cpp
    GeometryActorManagerSelectOp.cpp
    InteractionService.cpp
    TopologyDiagnosticActor.cpp
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
target_link_libraries(vtkPart PUBLIC AlgorithmSystem) # 交互公共类型头文件随 vtkPart 头文件传播（经 SystemCore）
target_link_libraries(vtkPart
    PRIVATE
        Data
        TKIVtk TKDESTEP
        VTK::InteractionStyle VTK::FiltersGeometry VTK::FiltersExtraction
        VTK::FiltersSources
        VTK::FiltersTexture
        VTK::RenderingAnnotation
)
vtk_module_autoinit(TARGETS vtkPart MODULES VTK::InteractionStyle VTK::FiltersGeometry VTK::FiltersExtraction VTK::FiltersSources VTK::FiltersTexture VTK::RenderingAnnotation)

qt_add_qml_module(renderQml
    URI app.render
    STATIC
    SOURCES QRenderWindow.h QRenderWindow.cpp
)
target_link_libraries(renderQml PRIVATE 
    vtkPart modelQml 
    FeatureSystem # startInteraction 经功能系统获取 InteractionState
    VTK::GUISupportQtQuick VTK::CommonColor VTK::RenderingAnnotation
)
vtk_module_autoinit(TARGETS renderQml MODULES 
    VTK::GUISupportQtQuick VTK::CommonColor VTK::RenderingAnnotation
)
add_subdirectory(test)
