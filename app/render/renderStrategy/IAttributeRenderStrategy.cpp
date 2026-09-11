#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"
#include <vtkDataSetAttributes.h>

void IAttributeRenderStrategy::cancelActiveAttribute(AttributeOperator op)
{
    op.disableFaceAttributeOffset();
    if (op.getFaceActor()->GetTexture() != nullptr) {
        op.getFaceActor()->SetTexture(nullptr);
    }
    op.getGlyph3DActor()->SetVisibility(0);

    // 关闭属性渲染时同时解除数据上的 active scalar，避免后续复用 PolyData 的
    // 组件高亮等 mapper 继续读取上一次的属性颜色。
    op.getFacePointData()->SetActiveAttribute(-1, vtkDataSetAttributes::SCALARS);
    op.getSolidPointData()->SetActiveAttribute(-1, vtkDataSetAttributes::SCALARS);
    op.getEdgeCellData()->SetActiveAttribute(-1, vtkDataSetAttributes::SCALARS);
    op.getFaceCellData()->SetActiveAttribute(-1, vtkDataSetAttributes::SCALARS);
    op.getSolidCellData()->SetActiveAttribute(-1, vtkDataSetAttributes::SCALARS);

    op.getEdgeMapper()->SetScalarVisibility(0);
    op.getFaceMapper()->SetScalarVisibility(0);
    op.getSolidMapper()->SetScalarVisibility(0);
}
