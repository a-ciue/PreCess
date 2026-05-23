#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "IncrementalMeshContext.h"
#include <TopoDS_Shape.hxx>

GmshIncrementalMeshState::GmshIncrementalMeshState() = default;
GmshIncrementalMeshState::~GmshIncrementalMeshState() = default;

void GmshIncrementalMeshState::clear()
{
    meshContext.reset();
    meshedEdgesCache.clear();
    meshedFacesCache.clear();
    meshedEdgeRefCounts.clear();
    local_to_global_point_ids.clear();
}

GeometryData::GeometryData() = default;
GeometryData::~GeometryData() = default;

std::optional<GeometryDataVtk> GeometryData::getGeometryData()
{
	GeometryDataVtk modelData { *this->rootShape };
	return modelData;
} 

void GeometryData::ensureIndexBuilt(GeometryRegistry& reg)
{
    if (!rootShape)
        return;
    if (!index.built)
        index.build(*rootShape, reg);
}