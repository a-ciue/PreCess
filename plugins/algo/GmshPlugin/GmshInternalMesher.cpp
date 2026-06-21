#include "GmshInternalMesher.h"

#include "GEdge.h"
#include "GModel.h"

namespace GmshInternalMesher {

int findEdgeTag(const TopoDS_Edge& edge)
{
    GModel* model = GModel::current();
    if (!model)
        return 0;

    GEdge* gmshEdge = model->getEdgeForOCCShape(static_cast<const void*>(&edge));
    return gmshEdge ? gmshEdge->tag() : 0;
}

} // namespace GmshInternalMesher
