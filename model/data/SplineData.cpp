#include "SplineData.h"
#include "SplineDataVtk.h"
#include <TopoDS_Shape.hxx>

SplineData::SplineData() = default;
SplineData::~SplineData() = default;

std::optional<SplineDataVtk> SplineData::getSplineData()
{
	SplineDataVtk modelData { *this->rootShape };
	return modelData;
} 

void SplineData::ensureCadIndexBuilt(GeometryRegistry& reg)
{
    if (!rootShape)
        return;
    if (!cad_index.built)
        cad_index.build(*rootShape, reg);
}