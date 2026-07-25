/**
 * @file CgalMeshAdapter.h
 * @brief MeshData 与 CGAL Surface_mesh 的双向转换
 *
 * 本库依赖 CGAL（GPLv3）。按项目许可证分界，仅允许 plugins/（AGPLv3）内使用，
 * 禁止 core/、model/、cmake/ 目录依赖本库或 CGAL 头文件。
 */
#ifndef CGAL_MESH_ADAPTER_H
#define CGAL_MESH_ADAPTER_H

#include "MeshData.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>

//! @brief CGAL 网格内核：EPIC（无 GMP 时精确数类型回退 CGAL::MP_Float，正确性不变、性能略降）
using CgalKernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalPoint3 = CgalKernel::Point_3;
using CgalMesh = CGAL::Surface_mesh<CgalPoint3>;

/**
 * @brief MeshData 转 CGAL Surface_mesh
 * @param mesh 源网格（当前版本仅接受全三角面网格，PMP 算法普遍要求三角网格）
 * @param global_points 模型全局点数组（ModelLayer::globalPoints()）。
 *        数据约定：mesh.local_to_global_ 非空时（模型树中的网格），vertex_positions_ 已被
 *        ModelLayer 清空，顶点坐标与面索引（全局点 id）均经 global_points 解析；
 *        为空时（如刚导入未入库的网格）直接使用 mesh.vertex_positions_ 局部索引
 * @return 转换后的 Surface_mesh（顶点索引与 MeshData 局部索引一致）
 * @throw std::runtime_error 含非三角面、面顶点索引越界或全局 id 不在组件内/越界时抛出
 */
CgalMesh toSurfaceMesh(const MeshData& mesh, const std::vector<std::array<double, 3>>& global_points);

/**
 * @brief CGAL Surface_mesh 转回 MeshData（覆盖 out 的顶点与面数据，输出为局部索引约定，local_to_global_ 清空）
 * @param sm 源网格
 * @param out 目标 MeshData；顶点索引按遍历序重排（源网格删除操作后索引可能不连续）
 */
void fromSurfaceMesh(const CgalMesh& sm, MeshData& out);

#endif // CGAL_MESH_ADAPTER_H
