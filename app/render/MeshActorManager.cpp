#include "MeshActorManager.h"
#include "Core.h"
#include "renderStrategy/AttriRenderStrategyScalar.h"
#include "renderStrategy/AttriRenderStrategyVector.h"
#include "renderStrategy/AttriRenderStrategyUV.h"
#include "renderStrategy/AttriRenderStrategyRGB.h"
#include <spdlog/spdlog.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

MeshActorManager::MeshActorManager(vtkPoints* global_points)
    : global_points_(global_points)
{
    scalar_bar_->SetOrientationToVertical(); // 使用竖向颜色表。
    scalar_bar_->SetPosition(0.90, 0.05); // 归一化视口坐标，原点位于渲染窗口左下角。
    scalar_bar_->SetWidth(0.05); // 颜色表宽度占视口宽度。
    scalar_bar_->SetHeight(0.20); // 颜色表高度占视口高度。
    scalar_bar_->SetNumberOfLabels(5); // 显示 5 个标量刻度值。
    scalar_bar_->SetLabelFormat("%.3g"); // 使用 3 位有效数字显示刻度值。
    scalar_bar_->SetUnconstrainedFontSize(true); // 使用下面指定的固定字体大小。
    scalar_bar_->GetLabelTextProperty()->SetFontSize(13); // 刻度字体大小。
    scalar_bar_->GetTitleTextProperty()->SetFontSize(15); // 属性名标题字体大小。
    scalar_bar_->GetLabelTextProperty()->SetColor(0.0, 0.0, 0.0); // 刻度文字使用黑色。
    scalar_bar_->GetTitleTextProperty()->SetColor(0.0, 0.0, 0.0); // 属性名标题使用黑色。
    scalar_bar_->SetVisibility(false); // 默认隐藏，仅在标量渲染时显示。
}

std::shared_ptr<const MeshActor> MeshActorManager::getComponentActor(Index component_id) const
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_.at(component_id);
    else {
        spdlog::error("MeshActorManager getComponentActor error");
        return nullptr;
    }
}

void MeshActorManager::deleteComponent(Index component_id)
{
    if (this->component_actors_.count(component_id)) {
        if (scalar_bar_component_id_ == component_id) {
            scalar_bar_->SetVisibility(false);
            scalar_bar_component_id_ = -1;
        }
        this->component_actors_.erase(component_id);
    }
}

void MeshActorManager::bindRender(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
    if (renderer_)
        renderer_->AddViewProp(scalar_bar_);
}

bool MeshActorManager::hasComponent(Index component_id) const
{
    return this->component_actors_.count(component_id) != 0;
}

void MeshActorManager::loadMesh(Index component_id, const MeshDataVtk& model_data, vtkRenderer* renderer, ModelRenderMode render_mode)
{
    if (!this->component_actors_.count(component_id))
        this->component_actors_[component_id] = std::make_shared<MeshActor>(
            renderer, global_points_, true, ModelRenderMode::Face);

    if (scalar_bar_component_id_ == component_id) {
        scalar_bar_->SetVisibility(false);
        scalar_bar_component_id_ = -1;
    }
    auto& actor = this->component_actors_[component_id];
    actor->loadModelData(model_data);
    actor->setRenderMode(render_mode);
}

void MeshActorManager::setVisibility(Index component_id, bool visibility)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setVisibility(visibility);
}

void MeshActorManager::setRenderMode(Index component_id, ModelRenderMode render_mode)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setRenderMode(render_mode);
}

void MeshActorManager::setRenderEdge(Index component_id, bool is_render)
{
    if (this->component_actors_.count(component_id))
        this->component_actors_[component_id]->setRenderEdge(is_render);
}

void MeshActorManager::setClipPlane(vtkPlane* plane)
{
    for (auto&& [idx, mesh_actor] : this->component_actors_) {
        mesh_actor->setClipPlane(plane);
    }
}

bool MeshActorManager::getCount(Index component_id)
{
    return this->component_actors_.count(component_id);
}


bool MeshActorManager::getIsEdgeRender(Index component_id)
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_[component_id]->getIsEdgeRender();
    return false;
}

ModelRenderMode MeshActorManager::getMeshRenderMode(Index component_id)
{
    if (this->component_actors_.count(component_id))
        return this->component_actors_[component_id]->getMeshRenderMode();

    return ModelRenderMode::Face;
}

void MeshActorManager::setAttriMode(
    Index component_id,
    const std::string& attr_name,
    Mode mode,
    std::map<std::string, std::any> args)
{
    if (this->component_actors_.count(component_id)) {
        std::unique_ptr<IAttributeRenderStrategy> strategy;
        switch (mode) {
        case Mode::SCALAR:
            strategy = std::make_unique<AttriRenderStrategyScalar>(scalar_bar_);
            break;
        case Mode::VECTOR:
            strategy = std::make_unique<AttriRenderStrategyVector>();
            break;
        case Mode::UV:
            strategy = std::make_unique<AttriRenderStrategyUV>();
            break;
        case Mode::RGB:
            strategy = std::make_unique<AttriRenderStrategyRGB>();
            break;
        default:
            spdlog::error("Invalid attribute render mode");
            return;
        }

        // 颜色表为窗口级共享对象，执行新策略前清理上一次标量渲染状态。
        scalar_bar_->SetVisibility(false);
        scalar_bar_component_id_ = -1;
        this->component_actors_[component_id]->setRenderStrategy(std::move(strategy));
        this->component_actors_[component_id]->renderAttribute(attr_name, args);
        scalar_bar_component_id_ = scalar_bar_->GetVisibility() ? component_id : -1;
    }
}

void MeshActorManager::cancelAttri(Index component_id)
{
    if (this->component_actors_.count(component_id)) {
        this->component_actors_[component_id]->cancelActiveAttribute();
        if (scalar_bar_component_id_ == component_id) {
            scalar_bar_->SetVisibility(false);
            scalar_bar_component_id_ = -1;
        }
    }
}

void MeshActorManager::syncOriginalPointIds()
{
    for (auto& [cid, actor] : component_actors_) {
        if (!actor)
            continue;
        actor->ensureOriginalPointIds(); 
    }
}
