#ifndef SPLINE_DATA_VTK_H
#define SPLINE_DATA_VTK_H
class TopoDS_Shape;
struct SplineDataVtk
{
    TopoDS_Shape& shape;
    Index component_id { -1 };
};
#endif // !SPLINE_DATA_VTK
