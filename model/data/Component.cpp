#include "Component.h"
#include "MeshData.h"
#include "SplineData.h"

Component::Component() = default;
Component::~Component() = default;

bool Component::hasMesh() const noexcept 
{
    return static_cast<bool>(mesh); 
}

bool Component::hasCad() const noexcept 
{ 
    return static_cast<bool>(cad);
}

SplineMeshMap& Component::ensureMapping()
{
    if (!mapping)
        mapping = std::make_unique<SplineMeshMap>();
    return *mapping;
}

const SplineMeshMap* Component::getMapping() const noexcept 
{ 
    return mapping.get(); 
}

bool Component::ownsGlobalPoint(Index global_pid) const noexcept
{
    if (!mesh)
        return false;
    const Index base = mesh->global_point_base_;
    const Index cnt = mesh->vertex_count_;
    return base >= 0 && cnt >= 0 && global_pid >= base && global_pid < base + cnt;
}