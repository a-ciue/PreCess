#include "ComponentData.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "MeshIDMap.h"

ComponentData::ComponentData() = default;
ComponentData::~ComponentData() = default;

bool ComponentData::hasMesh() const noexcept 
{
    return static_cast<bool>(mesh); 
}

bool ComponentData::hasGeometry() const noexcept 
{ 
    return static_cast<bool>(geometry);
}

GeometryMeshMap& ComponentData::ensureMapping()
{
    if (!mapping)
        mapping = std::make_unique<GeometryMeshMap>();
    return *mapping;
}

void ComponentData::ensurePointGlobalIds(MeshIDMap& map)
{
    if (!mesh)
        return;

    const Index count = mesh->vertex_count_;
    if (static_cast<Index>(point_global_ids_.size()) < count)
        point_global_ids_.resize(static_cast<size_t>(count), -1);

    // 幂等补缺：只为尚未分配的局部点申请 gid
    for (Index local = 0; local < count; ++local) {
        if (point_global_ids_[static_cast<size_t>(local)] < 0)
            point_global_ids_[static_cast<size_t>(local)] = map.insert(id, local);
    }
}

void ComponentData::releasePointGlobalIds(MeshIDMap& map)
{
    for (Index gid : point_global_ids_) {
        if (gid >= 0)
            map.remove(gid);
    }
    point_global_ids_.clear();
}