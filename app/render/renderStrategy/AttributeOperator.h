#pragma once
#include "MeshActor.h"
#include <vtkPointData.h>
#include <vtkCellData.h>

class MeshActor;
/**
 * @brief 属性操作类，提供对MeshActor中各类数据和Mapper、Actor的访问接口
 * @author yh
 */
class AttributeOperator {
public:
    AttributeOperator(MeshActor* mesh_actor_);

    vtkPolyDataMapper* getVertexMapper();
    vtkPolyDataMapper* getFaceMapper();
    vtkPolyDataMapper* getGlyph3DMapper();

    vtkActor* getGlyph3DActor();
    vtkActor* getFaceActor();

    vtkCellData* getFaceCellData();
    vtkPointData* getFacePointData();
    vtkPointData* getVertexPointData();

    vtkSmartPointer<vtkPolyData> getFaceGlyphInput(const std::string& attr_name);
    vtkSmartPointer<vtkPolyData> getVertexGlyphInput(const std::string& attr_name);

private:
    MeshActor* mesh_actor_;
};