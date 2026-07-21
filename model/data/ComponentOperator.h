#pragma once
#include "Core.h"

class ModelLayer;
class ModelObserver;
struct ComponentData;
struct MeshData;
struct GeometryData;
class ModelData;
class TopoDS_Shape;

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

    /**
     * @brief 将新形状写入当前组件；无几何时初始化，否则追加并重建子形状索引。
     * @return 当前组件 ID。
     */
    Index appendGeometryShape(TopoDS_Shape shape);

private:
    Index component_id_ { -1 };
    Index model_id_ { -1 };
    ComponentData* component_ { nullptr };
    ModelLayer* mgr_ { nullptr };
    ModelObserver* observer_ { nullptr };
};