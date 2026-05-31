#include "GmshIncrementalMeshState.h"

#include "IncrementalMeshContext.h"

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
