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

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>

//! @brief CGAL 网格内核：EPIC（无 GMP 时精确数类型回退 CGAL::MP_Float，正确性不变、性能略降）
using CgalKernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalPoint3 = CgalKernel::Point_3;
using CgalMesh = CGAL::Surface_mesh<CgalPoint3>;

//! @brief CGAL 网格内核：EPECK（谓词与构造均精确）。
//! 无 GMP 时精确数类型回退为 Lazy_exact_nt<Quotient<MP_Float>>，正确性不变、性能显著下降。
//! 网格布尔（corefinement）等对相交点依赖精确构造的算法应使用该内核避免浮点误差导致的错误结果。
using CgalExactKernel = CGAL::Exact_predicates_exact_constructions_kernel;
using CgalExactPoint3 = CgalExactKernel::Point_3;
using CgalExactMesh = CGAL::Surface_mesh<CgalExactPoint3>;

/**
 * @brief MeshData 转 CGAL Surface_mesh
 * @param mesh 源网格（当前版本仅接受全三角面网格，PMP 算法普遍要求三角网格）。
 *        数据约定：MeshData 自包含，坐标直读 vertex_positions_，面顶点为组件内局部点索引
 * @return 转换后的 Surface_mesh（顶点索引与 MeshData 局部索引一致）
 * @throw std::runtime_error 含非三角面或面顶点索引越界时抛出
 */
CgalMesh toSurfaceMesh(const MeshData& mesh);

/**
 * @brief MeshData 转 CGAL Surface_mesh（EPECK 精确构造内核版）
 *
 * 与 toSurfaceMesh 相同的输入约定与校验，输出网格顶点坐标为输入 double 坐标的精确表示。
 * 用于 CGAL corefinement 等对相交点要求精确构造的布尔运算。
 */
CgalExactMesh toExactSurfaceMesh(const MeshData& mesh);

/**
 * @brief CGAL Surface_mesh 转回 MeshData（覆盖 out 的顶点与面数据，输出为局部索引约定）
 * @param sm 源网格
 * @param out 目标 MeshData；顶点索引按遍历序重排（源网格删除操作后索引可能不连续）
 */
void fromSurfaceMesh(const CgalMesh& sm, MeshData& out);

/**
 * @brief CGAL Surface_mesh（EPECK 精确内核）转回 MeshData（语义同 fromSurfaceMesh）
 */
void fromSurfaceMesh(const CgalExactMesh& sm, MeshData& out);

#endif // CGAL_MESH_ADAPTER_H
