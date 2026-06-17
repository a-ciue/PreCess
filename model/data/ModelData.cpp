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

ModelData::ModelData(ModelData&& other) noexcept = default;
ModelData& ModelData::operator=(ModelData&& other) noexcept = default;

const std::vector<Index>& ModelData::componentIds() const noexcept 
{ 
    return component_ids_; 
}
std::vector<Index>& ModelData::componentIds() noexcept 
{ 
    return component_ids_; 
}
