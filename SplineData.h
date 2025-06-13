#pragma once
#include <vector>
#include <QString>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
//#include <TopoDS.hxx>

// 以后需要控制点 / 曲率等，可继续添加字段
struct SplineData {
    TopoDS_Shape rootShape;      // 读取 STEP/IGES 后的拓扑根
    std::vector<TopoDS_Edge>     edges;      // 可选：拆分得到的边
    std::vector<TopoDS_Vertex>   controlPts; // 可选：用于渲染／算法
    QString  sourcePath;                       // 原始文件
    int      degree{3};                        // 默认三次样条
    bool     closed{false};
    QString model_name_;
};
