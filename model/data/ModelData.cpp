/**
 * @file ModelData.cpp
 * @brief 实现 ModelData 类的核心功能，用于管理和操作网格数据
 *
 * 该文件包含 ModelData 类的实现，提供网格数据的存储、更新和操作功能，包括：
 * - 读取和写入网格数据
 * - 面和边的分割
 * - 块和组的合并
 * - 重新网格化功能
 * - 维护与 ModelActor 及 VTK 组件的交互
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/8
 */

#include "ModelData.h"
#include "MeshData.h"
#include "GeometryData.h"

#include <stdexcept>  // 用于抛出异常
#include <algorithm>
#include <spdlog/spdlog.h>

ModelData::ModelData() = default;
ModelData::~ModelData() = default;

ModelData::ModelData(std::unique_ptr<MeshData> mesh)
{
    ComponentData* c = createComponent(-1, "Comp_0");
    c->mesh = std::move(mesh);
    spdlog::info("ModelData: created 1 component with mesh");
}

ModelData::ModelData(std::unique_ptr<GeometryData> geometry)
{
    ComponentData* c = createComponent(-1, "Comp_0");
    c->geometry = std::move(geometry);
    spdlog::info("ModelData: created 1 component with geometry");
}

ModelData::ModelData(ModelData&& other) noexcept = default;
ModelData& ModelData::operator=(ModelData&& other) noexcept = default;

// 创建组件
ComponentData* ModelData::createComponent(Index id, const std::string& name)
{
    auto c = std::make_unique<ComponentData>();
    c->id = id;
    c->name = name;

    components_.push_back(std::move(c));
    return components_.back().get();
}
const std::vector<Index>& ModelData::componentIds() const noexcept 
{ 
    return component_ids_; 
}
std::vector<Index>& ModelData::componentIdsMut() noexcept 
{ 
    return component_ids_; 
}
std::vector<std::unique_ptr<ComponentData>>& ModelData::stagingcomponents()
{ 
    return components_; 
}
const std::vector<std::unique_ptr<ComponentData>>& ModelData::stagingcomponents() const
{ 
    return components_; 
}