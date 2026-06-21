#include "GmshIncrementalMeshState.h"

GmshIncrementalMeshState::GmshIncrementalMeshState() = default;
GmshIncrementalMeshState::~GmshIncrementalMeshState() = default;

void GmshIncrementalMeshState::clear()
{
    meshedEdgesCache.clear();
    meshedFacesCache.clear();
    meshedEdgeRefCounts.clear();
    local_to_global_point_ids.clear();
}
