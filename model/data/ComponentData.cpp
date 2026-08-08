#include "ComponentData.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "MeshIDMap.h"

ComponentData::ComponentData() = default;
ComponentData::~ComponentData() = default;

std::unique_ptr<ComponentData> ComponentData::clone() const
{
    auto copy = std::make_unique<ComponentData>();
    copy->id = id;
    copy->name = name;

    if (mesh)
        copy->mesh = mesh->clone();
    if (geometry)
        copy->geometry = geometry->clone();
    if (mapping)
        copy->mapping = std::make_unique<GeometryMeshMap>(*mapping);

    // 自定义拷贝语义：只带持久身份层（稳定边 id/gid 表），懒表自动置 dirty
    copy->mesh_adjacency = mesh_adjacency;

    copy->point_global_ids_ = point_global_ids_;
    copy->material_id = material_id;
    copy->source_xde_leaf_id = source_xde_leaf_id;
    return copy;
}

void ComponentData::restoreFrom(const ComponentData& snapshot)
{
    // id 保留本组件原值，其余全部按快照覆盖
    name = snapshot.name;
    mesh = snapshot.mesh ? snapshot.mesh->clone() : nullptr;
    geometry = snapshot.geometry ? snapshot.geometry->clone() : nullptr;
    mapping = snapshot.mapping ? std::make_unique<GeometryMeshMap>(*snapshot.mapping) : nullptr;
    mesh_adjacency = snapshot.mesh_adjacency;
    point_global_ids_ = snapshot.point_global_ids_;
    material_id = snapshot.material_id;
    source_xde_leaf_id = snapshot.source_xde_leaf_id;
}

void ComponentData::reclaimPointGlobalIds(MeshIDMap& map)
{
    for (Index local = 0; local < static_cast<Index>(point_global_ids_.size()); ++local) {
        const Index gid = point_global_ids_[static_cast<size_t>(local)];
        if (gid >= 0)
            map.reclaim(gid, id, local);
    }
}

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