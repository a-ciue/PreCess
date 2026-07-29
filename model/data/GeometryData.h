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
    GeometrySubshapeIndex index;

    /**
     * @brief 将单个形状设置为严格一层扁平的根 Compound。
     *
     * 输入中的 Compound 会递归展开，保证根节点的直接子节点不再包含 Compound。
     */
    void setRootShape(TopoDS_Shape shape);

    /**
     * @brief 将形状追加到根 Compound，并保持根节点严格一层扁平。
     *
     * 调用方需要在追加前释放已经建立的子形状索引。
     */
    void appendRootShape(TopoDS_Shape shape);

    void ensureIndexBuilt(GeometryRegistry& reg);

    //std::vector<TopoDS_Edge>     edges;      // 可选：拆分得到的边
    //std::vector<TopoDS_Vertex>   controlPts; // 可选：用于渲染／算法

    std::optional<GeometryDataVtk> getGeometryData();
};
