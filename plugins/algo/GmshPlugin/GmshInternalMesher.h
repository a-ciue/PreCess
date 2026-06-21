#pragma once

class TopoDS_Edge;

namespace GmshInternalMesher {

// 通过 Gmsh 内部 OCC Shape 映射返回曲线 tag；未找到时返回 0。
// 必须在 importShapesNativePointer() 和 synchronize() 之后调用。
int findEdgeTag(const TopoDS_Edge& edge);

} // namespace GmshInternalMesher
