#ifndef GEOMETRY_DATA_VTK_H
#define GEOMETRY_DATA_VTK_H
class TopoDS_Shape;
struct GeometrySubshapeIndex;
struct GeometryDataVtk
{
    const TopoDS_Shape& shape;
    Index component_id { -1 };
    const GeometrySubshapeIndex* geometry_index { nullptr };
};
#endif // !GEOMETRY_DATA_VTK_H
