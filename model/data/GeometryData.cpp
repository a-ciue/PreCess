#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include <TopoDS_Shape.hxx>

GeometryData::GeometryData() = default;
GeometryData::~GeometryData() = default;

std::optional<GeometryDataVtk> GeometryData::getSplineData()
{
	GeometryDataVtk modelData { *this->rootShape };
	return modelData;
} 

void GeometryData::ensureCadIndexBuilt(GeometryRegistry& reg)
{
    if (!rootShape)
        return;
    if (!cad_index.built)
        cad_index.build(*rootShape, reg);
}