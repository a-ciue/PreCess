#pragma once
#include "Core.h"

#include <unordered_set>
#include <utility>
#include <vector>

class MeshIDMap {
public:
    using ComponentID = Index;
    using LocalID = Index; // localEdgeId
    using GlobalID = Index; // globalEdgeId

    static constexpr ComponentID kInvalidComponent = -1;
    static constexpr LocalID kInvalidLocal = -1;

    GlobalID insert(ComponentID component_id, LocalID local_id);
    std::vector<GlobalID> insertRange(ComponentID component_id, LocalID local_begin, LocalID count);

    bool remove(GlobalID global_id);
    std::pair<ComponentID, LocalID> getLocal(GlobalID global_id) const;

    size_t size() const;
    size_t freeSize() const;

private:
    std::vector<std::pair<ComponentID, LocalID>> global_to_local_; // index=GlobalID
    std::unordered_set<GlobalID> free_ids_; // 可复用id集合
};