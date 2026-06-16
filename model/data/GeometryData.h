#pragma once
#include <memory>
#include <optional>
#include "GeometrySubshapeIndex.h"

class TopoDS_Shape;
struct GeometryDataVtk;

// 以后需要控制点 / 曲率等，可继续添加字段
struct GeometryData {
    GeometryData();
	~GeometryData();
    std::unique_ptr<TopoDS_Shape> rootShape;      // 读取 STEP/IGES 后的拓扑根
    GeometrySubshapeIndex geometry_index;
    void ensureGeometryIndexBuilt(GeometryRegistry& reg);

    //std::vector<TopoDS_Edge>     edges;      // 可选：拆分得到的边
    //std::vector<TopoDS_Vertex>   controlPts; // 可选：用于渲染／算法

    std::optional<GeometryDataVtk> getGeometryVtkData();
};
