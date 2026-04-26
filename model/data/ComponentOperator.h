#pragma once
#include "Core.h"

class ModelManager;
class ModelObserver;
struct Component;
struct MeshData;
struct SplineData;
class ModelData;

class ComponentOperator {
public:
    ComponentOperator(Index component_id,
        Component& component,
        ModelManager& mgr,
        ModelObserver* observer) noexcept;

    Index componentId() const noexcept { return component_id_; }

    Component& component() const noexcept { return *component_; }
    MeshData* mesh() const noexcept;
    SplineData* cad() const noexcept;

    ModelManager& manager() const noexcept { return *mgr_; }
    ModelObserver* observer() const noexcept { return observer_; }

    // 常用：找到归属的 model（用于兜底通知/算法上下文）
    Index modelId() const; // 找不到就抛异常或返回 -1（二选一）
    ModelData* model() const; // 可能为空

    // 通知
    void notifyChanged() const;

private:
    Index component_id_ { -1 };
    Component* component_ { nullptr };
    ModelManager* mgr_ { nullptr };
    ModelObserver* observer_ { nullptr };
};