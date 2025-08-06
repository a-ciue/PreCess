#pragma once
#include <memory>
#include <optional>

class TopoDS_Shape;
struct SplineDataVtk;

// 以后需要控制点 / 曲率等，可继续添加字段
struct SplineData {
    SplineData();
	~SplineData();
    std::unique_ptr<TopoDS_Shape> rootShape;      // 读取 STEP/IGES 后的拓扑根
    //std::vector<TopoDS_Edge>     edges;      // 可选：拆分得到的边
    //std::vector<TopoDS_Vertex>   controlPts; // 可选：用于渲染／算法
    int      degree{3};                        // 默认三次样条
    bool     closed{false};

    std::optional<SplineDataVtk> getSplineData();
};
