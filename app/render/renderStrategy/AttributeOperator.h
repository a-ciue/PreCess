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

    vtkPolyDataMapper* getFaceMapper();
    vtkPolyDataMapper* getSolidMapper();
    vtkPolyDataMapper* getGlyph3DMapper();

    vtkActor* getGlyph3DActor();
    vtkActor* getFaceActor();

    vtkCellData* getFaceCellData();
    vtkPointData* getFacePointData();
    vtkCellData* getSolidCellData();
    vtkPointData* getSolidPointData();

    // 启用面属性渲染的深度偏移，避免面属性和体外表面共面时互相遮挡。
    void enableFaceAttributeOffset();

    // 关闭面属性渲染的深度偏移，恢复普通面渲染深度关系。
    void disableFaceAttributeOffset();

    // 显示标量颜色表，并让颜色表与 mapper 使用相同的颜色映射和值域。
    void showScalarBar(
        vtkPolyDataMapper* mapper,
        const std::string& title,
        const double range[2]);

    // 隐藏当前渲染窗口的标量颜色表。
    void hideScalarBar();

    // 返回当前 component 的典型边长，用于计算 glyph 默认缩放比例。
    double getMeshScale() const noexcept;

    vtkSmartPointer<vtkPolyData> getFaceGlyphInput(const std::string& attr_name);
    vtkSmartPointer<vtkPolyData> getPointGlyphInput(const std::string& attr_name);
    vtkSmartPointer<vtkPolyData> getSolidGlyphInput(const std::string& attr_name);

private:
    MeshActor* mesh_actor_;
};
