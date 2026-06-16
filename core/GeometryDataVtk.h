#ifndef GEOMETRY_DATA_VTK_H
#define GEOMETRY_DATA_VTK_H
class TopoDS_Shape;
struct GeometryDataVtk
{
    TopoDS_Shape& shape;
    Index component_id { -1 };
};
#endif // !GEOMETRY_DATA_VTK_H
