#pragma once
#include "MeshActor.h"
#include <vtkUnstructuredGrid.h> 

class MeshActor;
/**
 * @brief 属性操作类，提供对MeshActor中各类数据和Mapper、Actor的访问接口
 * @author yh
 */
class AttributeOperator {
public:
    AttributeOperator(MeshActor* mesh_actor_);

    vtkPolyDataMapper* getVertexMapper();
    vtkPolyDataMapper* getEdgeMapper();
    vtkPolyDataMapper* getFaceMapper();
    vtkPolyDataMapper* getSolidMapper();
    vtkPolyDataMapper* getGlyph3DMapper();

    vtkActor* getSolidActor();
    vtkActor* getFaceActor();
    vtkActor* getEdgeActor();
    vtkActor* getVertexActor();
    vtkActor* getGlyph3DActor();

    vtkUnstructuredGrid* getSolidData();
    vtkPolyData* getFaceData();
    vtkPolyData* getEdgeData();
    vtkPolyData* getVertexData();

private:
    MeshActor* mesh_actor_;
};