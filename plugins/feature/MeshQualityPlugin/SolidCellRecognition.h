/**
 * @file SolidCellRecognition.h
 * @brief 保留原单元身份，按有向面拓扑识别折叠后的标准线性体单元。
 */
#pragma once

#include "Core.h"

#include <span>
#include <vector>

namespace systems::feature {

/** @brief 单元拓扑识别状态；只描述类型识别，不代表几何质量合格。 */
enum class SolidCellRecognition {
    Original = 0, //> 沿用标准单元类型
    Collapsed = 1, //> 折叠后的面拓扑匹配另一种标准单元
    Unrecognized = 2, //> 折叠后无法匹配支持的标准单元
    Unsupported = 3, //> 原始类型尚不支持拓扑识别
};

/**
 * @brief 一条原始体单元的识别记录；point_ids 为计算类型要求的组件局部节点顺序。
 *
 * source_cell_id 为原始零基体单元下标；识别不改变 MeshData 的类型、连通性或编号。
 * 功能执行后将 source_cell_id、source_type、evaluated_type、recognition 分别保存为
 * s_mesh_quality_recognition_source_id_1、s_mesh_quality_recognition_source_type_1、
 * s_mesh_quality_recognition_evaluated_type_1、s_mesh_quality_recognition_status_1 属性，
 * 生命周期与本次功能生成的质量属性相同。evaluated_type 为 0 表示没有可计算类型。
 */
struct SolidCellRecord {
    Index source_cell_id { -1 };
    unsigned char source_type { 0 };
    unsigned char evaluated_type { 0 };
    SolidCellRecognition recognition { SolidCellRecognition::Unrecognized };
    std::vector<Index> point_ids;
};

/** @brief 返回支持的标准线性体单元角点数；未知类型返回 0。 */
Index solidPointCount(unsigned char cell_type);

/**
 * @brief 合并相邻重复角点后的面环，并以保留朝向的拓扑同构识别 Tet/Pyramid/Wedge。
 * @param source_cell_id 原始零基体单元下标。
 * @param source_type 原始 VTK 单元类型。
 * @param point_ids 原始组件局部节点连接表；只按重复节点身份折叠，不按坐标自动焊接。
 * @return 原始身份和计算节点映射；不按坐标翻转节点来修正倒置单元。
 */
SolidCellRecord recognizeSolidCell(Index source_cell_id, unsigned char source_type, std::span<const Index> point_ids);

}
