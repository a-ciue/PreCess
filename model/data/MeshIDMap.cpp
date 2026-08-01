#include "MeshIDMap.h"

#include <stdexcept>

MeshIDMap::GlobalID MeshIDMap::insert(ComponentID component_id, LocalID local_id)
{
    GlobalID gid = -1;

    if (!free_ids_.empty()) {
        auto it = free_ids_.begin();
        gid = *it;
        free_ids_.erase(it);
        global_to_local_[(size_t)gid] = { component_id, local_id };
        return gid;
    }

    gid = (GlobalID)global_to_local_.size();
    global_to_local_.push_back({ component_id, local_id });
    return gid;
}

std::vector<MeshIDMap::GlobalID> MeshIDMap::insertRange(ComponentID component_id, LocalID local_begin, LocalID count)
{
    std::vector<GlobalID> out;
    out.reserve((size_t)count);
    for (LocalID i = 0; i < count; ++i) {
        out.push_back(insert(component_id, local_begin + i));
    }
    return out;
}

void MeshIDMap::reclaim(GlobalID global_id, ComponentID component_id, LocalID local_id)
{
    if (global_id < 0 || (size_t)global_id >= global_to_local_.size())
        throw std::runtime_error("MeshIDMap::reclaim: global id out of range");

    auto it = free_ids_.find(global_id);
    if (it == free_ids_.end())
        throw std::runtime_error("MeshIDMap::reclaim: global id is occupied");

    free_ids_.erase(it);
    global_to_local_[(size_t)global_id] = { component_id, local_id };
}

bool MeshIDMap::remove(GlobalID global_id)
{
    if (global_id < 0 || (size_t)global_id >= global_to_local_.size())
        return false;

    auto& p = global_to_local_[(size_t)global_id];

    // 已经无效：不重复加入free pool
    if (p.first == kInvalidComponent && p.second == kInvalidLocal)
        return false;

    // 标记无效 + 进入复用池
    p = { kInvalidComponent, kInvalidLocal };
    free_ids_.insert(global_id);
    return true;
}

std::pair<MeshIDMap::ComponentID, MeshIDMap::LocalID>
MeshIDMap::getLocal(GlobalID global_id) const
{
    if (global_id < 0 || (size_t)global_id >= global_to_local_.size())
        return { kInvalidComponent, kInvalidLocal };

    return global_to_local_[(size_t)global_id];
}

size_t MeshIDMap::size() const 
{ 
    return global_to_local_.size(); 
}

size_t MeshIDMap::freeSize() const 
{ 
    return free_ids_.size(); 
}