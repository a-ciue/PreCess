#ifndef SELECTION_H
#define SELECTION_H
#include <vector>
#include "Core.h"

class ElementEnum {
public:
    enum Type
    {
        None,
        Solid,
        Face,
        Edge,
        Vertex,
        Block,
        Group,
        GeometrySolid,
        GeometryFace,
        GeometryEdge,
        GeometryVertex,
        Component
    };
};

//! @brief 存储选择的对象
struct Selection {
    //! @brief 选择对象的id序列
    //! @note Vertex 类型为全局点 id（MeshIDMap 分配，经 ModelLayer::pointIdMap()
    //!       可解析为 (component_id, 局部点 id)）；
    //!       Edge 类型为稳定局部边 id（跨拓扑编辑有效，物化边与面边同表），
    //!       id 查询缺失（防御路径）时为两个端点局部点 id 两两排列。
    std::vector<Index> ids;
    //! @brief 选择对象的类型
    ElementEnum::Type type;
    Index component_id { -1 };
};
#endif // SELECTION_H
