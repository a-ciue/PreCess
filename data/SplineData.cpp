#include "SplineData.h"
#include "core/SplineDataVtk.h"

std::optional<SplineDataVtk> SplineData::getSplineData()
{
	SplineDataVtk modelData { *this->rootShape };
	return modelData;
} 