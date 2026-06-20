#include "ComponentData.h"
#include "MeshData.h"
#include "GeometryData.h"

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