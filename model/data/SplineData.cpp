#include "SplineData.h"
#include "SplineDataVtk.h"

std::optional<SplineDataVtk> SplineData::getSplineData()
{
	SplineDataVtk modelData { *this->rootShape };
	return modelData;
} 