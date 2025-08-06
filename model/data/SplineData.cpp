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