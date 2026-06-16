#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include <TopoDS_Shape.hxx>

GeometryData::GeometryData() = default;
GeometryData::~GeometryData() = default;

std::optional<GeometryDataVtk> GeometryData::getGeometryVtkData()
{
	GeometryDataVtk modelData { *this->rootShape };
	return modelData;
} 

void GeometryData::ensureGeometryIndexBuilt(GeometryRegistry& reg)
{
    if (!rootShape)
        return;
    if (!geometry_index.built)
        geometry_index.build(*rootShape, reg);
}