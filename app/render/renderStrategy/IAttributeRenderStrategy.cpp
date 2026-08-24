#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"

void IAttributeRenderStrategy::cancelActiveAttribute(AttributeOperator op)
{
    op.disableFaceAttributeOffset();
    if (op.getFaceActor()->GetTexture() != nullptr) {
        op.getFaceActor()->SetTexture(nullptr);
    }
    op.getGlyph3DActor()->SetVisibility(0);
    op.getEdgeMapper()->SetScalarVisibility(0);
    op.getFaceMapper()->SetScalarVisibility(0);
    op.getSolidMapper()->SetScalarVisibility(0);
}
