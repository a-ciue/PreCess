#pragma once
#include "Core.h"

class ModelLayer;
class ModelObserver;
struct ComponentData;
struct MeshData;
struct GeometryData;
class ModelData;

class ComponentOperator {
public:
    ComponentOperator(Index component_id,
        ComponentData& component,
        ModelLayer& mgr,
        ModelObserver* observer = nullptr,
        Index model_id = -1) noexcept;

    Index componentId() const noexcept { return component_id_; }

    ComponentData& component() const noexcept { return *component_; }
    MeshData* mesh() const noexcept;
    GeometryData* geometry() const noexcept;

    ModelLayer& manager() const noexcept { return *mgr_; }
    ModelObserver* observer() const noexcept { return observer_; }

    Index modelId() const noexcept;
    ModelData* model() const;

    void notifyChanged() const;

    void removeMesh();
    void removeGeometry();

private:
    Index component_id_ { -1 };
    Index model_id_ { -1 };
    ComponentData* component_ { nullptr };
    ModelLayer* mgr_ { nullptr };
    ModelObserver* observer_ { nullptr };
};