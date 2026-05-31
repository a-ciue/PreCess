#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include <TopoDS_Shape.hxx>

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