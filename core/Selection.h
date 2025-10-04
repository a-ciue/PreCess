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
        Group
    };
};

//! @brief 存储选择的对象
struct Selection {
    //! @brief 选择对象的id序列
    std::vector<Index> ids;
    //! @brief 选择对象的类型
    ElementEnum::Type type;
    Index model_id;
};
#endif // SELECTION_H
