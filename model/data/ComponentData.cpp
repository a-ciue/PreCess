#include "ComponentData.h"
#include "MeshData.h"
#include "GeometryData.h"

ComponentData::ComponentData() = default;
ComponentData::~ComponentData() = default;

bool ComponentData::hasMesh() const noexcept 
{
    return static_cast<bool>(mesh); 
}

bool ComponentData::hasCad() const noexcept 
{ 
    return static_cast<bool>(geometry);
}

SplineMeshMap& ComponentData::ensureMapping()
{
    if (!mapping)
        mapping = std::make_unique<SplineMeshMap>();
    return *mapping;
}

const SplineMeshMap* ComponentData::getMapping() const noexcept 
{ 
    return mapping.get(); 
}

bool ComponentData::ownsGlobalPoint(Index global_pid) const noexcept
{
    if (!mesh)
        return false;
    const Index base = mesh->global_point_base_;
    const Index cnt = mesh->vertex_count_;
    return base >= 0 && cnt >= 0 && global_pid >= base && global_pid < base + cnt;
}