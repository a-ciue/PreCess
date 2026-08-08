#pragma once
#include "Core.h"

#include <unordered_set>
#include <utility>
#include <vector>

class MeshIDMap {
public:
    using ComponentID = Index;
    using LocalID = Index; // 稳定局部 id（如稳定局部边 id，非数组下标）
    using GlobalID = Index; // globalEdgeId

    static constexpr ComponentID kInvalidComponent = -1;
    static constexpr LocalID kInvalidLocal = -1;

    GlobalID insert(ComponentID component_id, LocalID local_id);
    std::vector<GlobalID> insertRange(ComponentID component_id, LocalID local_begin, LocalID count);

    /**
     * @brief 定向回收指定 gid（撤销/恢复快照时用，保持 gid 身份不变）
     * @throw std::runtime_error gid 越界或当前被占用（不在 free-list 中）
     */
    void reclaim(GlobalID global_id, ComponentID component_id, LocalID local_id);

    bool remove(GlobalID global_id);
    std::pair<ComponentID, LocalID> getLocal(GlobalID global_id) const;

    size_t size() const;
    size_t freeSize() const;

private:
    std::vector<std::pair<ComponentID, LocalID>> global_to_local_; // index=GlobalID
    std::unordered_set<GlobalID> free_ids_; // 可复用id集合
};