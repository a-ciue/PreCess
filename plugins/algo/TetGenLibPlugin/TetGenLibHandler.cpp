/**
 * @file TetGenLibHandler.cpp
 * @author 范成通 1941804585@qqin.com
 * @brief TetGen 库模式四面体剖分处理器实现，包含 MeshData↔tetgenio 双向转换与参数组装
 * @date 2026-06-24
 */
#include "TetGenLibHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <vtkCellType.h>

#include <tetgen.h>

namespace {
/**
 * @brief 组装 TetGen tetrahedralize 的开关字符串
 * @param preserve_surface 是否保留原始表面（对应 -Y）
 * @param detect_intersections_only 仅做自交检测（对应 -d），此时忽略其他参数
 * @param quality_bound 质量参数 q（半径/边长比上限），<=0 时不启用
 * @param max_volume 最大单元体积 a，<=0 时不启用
 * @return TetGen 开关字符串
 */
std::string makeTetGenSwitches(bool preserve_surface,
    bool detect_intersections_only,
    double quality_bound,
    double max_volume)
{
    std::string switches = "p";
    if (detect_intersections_only) {
        switches += "d";
        return switches;
    }

    if (quality_bound > 0.0) {
        switches += "q" + std::to_string(quality_bound);
    }
    if (max_volume > 0.0) {
        switches += "a" + std::to_string(max_volume);
    }
    if (preserve_surface) {
        switches += "Y";
    }
    switches += "Q";
    return switches;
}

/**
 * @brief 清理用户输入的附加 TetGen switches，移除空白和开头的 '-'
 * @param switches 用户输入的纯 switches 文本
 * @return 可追加到 TetGen switches 的文本
 */
std::string sanitizeExtraTetGenSwitches(std::string switches)
{
    switches.erase(
        std::remove_if(switches.begin(), switches.end(), [](unsigned char ch) {
            return std::isspace(ch);
        }),
        switches.end());

    while (!switches.empty() && switches.front() == '-') {
        switches.erase(switches.begin());
    }
    return switches;
}

/**
 * @brief 将模型名中的文件系统非法字符替换为下划线
 * @param name 原始名称
 * @return 清洗后的安全名称（为空时返回 Component）
 */
std::string sanitizeModelName(std::string name)
{
    if (name.empty()) {
        return "Component";
    }
    for (char& ch : name) {
        switch (ch) {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':
            ch = '_';
            break;
        default:
            break;
        }
    }
    return name;
}

/**
 * @brief 根据输入参数生成结果模型的自描述名称
 * @param input_component 输入的 Component（用于获取 name）
 * @param quality_bound q 参数值
 * @param max_volume a 参数值
 * @param preserve_surface 是否保留表面
 * @param use_largest_surface_shell 是否仅用最大壳
 * @return 形如 Name_TetGen_q1.2_a0_Y_largestShell 的名称
 */
std::string makeResultModelName(const ComponentData& input_component,
    double quality_bound,
    double max_volume,
    bool preserve_surface,
    bool use_largest_surface_shell)
{
    std::ostringstream name;
    name << sanitizeModelName(input_component.name) << "_TetGen";
    if (use_largest_surface_shell) {
        name << "_largestShell";
    }
    if (quality_bound > 0.0) {
        name << "_q" << quality_bound;
    }
    if (max_volume > 0.0) {
        name << "_a" << max_volume;
    }
    if (preserve_surface) {
        name << "_Y";
    }
    return name.str();
}

/**
 * @brief 替换当前 Component 的 mesh，维护全局点索引与边 ID 映射
 * @param comp 目标 Component 操作接口
 * @param mesh 新网格数据
 * @return true 成功；false 失败（Component 不存在或 mesh 为空）
 *
 * 释放旧 mesh 的 edge id map、追加新顶点到全局点池、转换本地→全局索引、
 * 重建 edge id map，最后通知 observer。与 ModelLayer::addModel 的 mesh 入池流程一致。
 */
bool replaceComponentMesh(ComponentOperator& comp, std::unique_ptr<MeshData> mesh)
{
    ModelLayer& mgr = comp.manager();
    const Index component_id = comp.componentId();
    ComponentData* component = mgr.findComponent(component_id);
    if (!component || !mesh) {
        return false;
    }

    if (component->mesh) {
        component->mesh_adjacency.releaseEdgeGlobalIds(mgr.edgeIdMap());
    }

    const Index base = mgr.appendGlobalPoints(mesh->vertex_positions_);
    mesh->vertex_count_ = static_cast<Index>(mesh->vertex_positions_.size());
    mesh->local_to_global_.resize(mesh->vertex_count_);
    for (Index i = 0; i < mesh->vertex_count_; ++i) {
        mesh->local_to_global_[static_cast<size_t>(i)] = base + i;
    }
    mesh->makePointIdsGlobal();
    std::vector<std::array<double, 3>> {}.swap(mesh->vertex_positions_);
    component->mesh_adjacency.ensureEdgeGlobalIds(mgr.edgeIdMap(), component_id, *mesh);

    component->mesh = std::move(mesh);
    comp.notifyChanged();
    return true;
}

/**
 * @brief 使用 BFS 找出网格表面中最大的连通分量（壳）
 *        通过顶点邻接表扩散连通面，跳过不相连的小腔体
 * @param mesh 输入网格
 * @return 最大连通壳的面索引列表（已排序）
 */
std::vector<Index> collectLargestSurfaceShellFaces(const MeshData& mesh)
{
    if (mesh.face_vertices_offset_.size() < 2) {
        return {};
    }
    const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
    if (face_count <= 1) {
        std::vector<Index> all_faces;
        for (Index face_id = 0; face_id < face_count; ++face_id) {
            all_faces.push_back(face_id);
        }
        return all_faces;
    }

    std::unordered_map<Index, std::vector<Index>> vertex_to_faces;
    for (Index face_id = 0; face_id < face_count; ++face_id) {
        const Index begin = mesh.face_vertices_offset_[static_cast<size_t>(face_id)];
        const Index end = mesh.face_vertices_offset_[static_cast<size_t>(face_id + 1)];
        for (Index offset = begin; offset < end; ++offset) {
            vertex_to_faces[mesh.face_vertices_[static_cast<size_t>(offset)]].push_back(face_id);
        }
    }

    std::vector<char> visited(static_cast<size_t>(face_count), 0);
    std::vector<Index> largest_component;

    for (Index seed = 0; seed < face_count; ++seed) {
        if (visited[static_cast<size_t>(seed)]) {
            continue;
        }

        std::vector<Index> component_faces;
        std::queue<Index> pending;
        pending.push(seed);
        visited[static_cast<size_t>(seed)] = 1;

        while (!pending.empty()) {
            const Index face_id = pending.front();
            pending.pop();
            component_faces.push_back(face_id);

            const Index begin = mesh.face_vertices_offset_[static_cast<size_t>(face_id)];
            const Index end = mesh.face_vertices_offset_[static_cast<size_t>(face_id + 1)];
            for (Index offset = begin; offset < end; ++offset) {
                const Index vertex_id = mesh.face_vertices_[static_cast<size_t>(offset)];
                auto it = vertex_to_faces.find(vertex_id);
                if (it == vertex_to_faces.end()) {
                    continue;
                }
                for (Index neighbor_face : it->second) {
                    if (!visited[static_cast<size_t>(neighbor_face)]) {
                        visited[static_cast<size_t>(neighbor_face)] = 1;
                        pending.push(neighbor_face);
                    }
                }
            }
        }

        if (component_faces.size() > largest_component.size()) {
            largest_component = std::move(component_faces);
        }
    }

    std::sort(largest_component.begin(), largest_component.end());
    return largest_component;
}

/**
 * @brief 收集网格的全部表面面索引
 * @param mesh 输入网格
 * @return 所有面的索引列表
 */
std::vector<Index> collectAllSurfaceFaces(const MeshData& mesh)
{
    if (mesh.face_vertices_offset_.size() < 2) {
        return {};
    }
    const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
    std::vector<Index> faces;
    faces.reserve(static_cast<size_t>(face_count));
    for (Index face_id = 0; face_id < face_count; ++face_id) {
        faces.push_back(face_id);
    }
    return faces;
}

/**
 * @brief 将 PreCess MeshData 转换为 TetGen tetgenio 输入结构
 *        处理顶点坐标复制、面转换、最大壳筛选
 * @param component 输入的 ComponentData（需有 MeshData）
 * @param manager ModelLayer 引用，用于获取全局点坐标
 * @param input 输出的 tetgenio 结构
 * @param use_largest_surface_shell 是否仅使用最大表面壳
 * @return true 成功；false 失败（无 mesh 或数据无效）
 */
bool buildTetGenInput(const ComponentData& component, const ModelLayer& manager, tetgenio& input, bool use_largest_surface_shell)
{
    const MeshData* mesh = component.asMeshData();
    if (!mesh) {
        spdlog::error("TetGenLibHandler: component has no mesh data");
        return false;
    }
    if (mesh->vertex_count_ <= 0 || mesh->local_to_global_.empty()) {
        spdlog::error("TetGenLibHandler: component mesh has no vertices");
        return false;
    }
    if (mesh->face_vertices_offset_.size() < 2 || mesh->face_vertices_.empty()) {
        spdlog::error("TetGenLibHandler: component mesh has no surface faces");
        return false;
    }

    const auto& global_points = manager.globalPoints();
    input.firstnumber = 0;
    input.mesh_dim = 3;
    input.numberofpoints = static_cast<int>(mesh->vertex_count_);
    input.pointlist = new REAL[input.numberofpoints * 3];
    input.pointmarkerlist = new int[input.numberofpoints];

    std::unordered_map<Index, Index> global_to_local;
    global_to_local.reserve(static_cast<size_t>(mesh->vertex_count_));

    for (Index local_id = 0; local_id < mesh->vertex_count_; ++local_id) {
        const Index global_id = mesh->local_to_global_[static_cast<size_t>(local_id)];
        if (global_id < 0 || static_cast<size_t>(global_id) >= global_points.size()) {
            spdlog::error("TetGenLibHandler: invalid global point id {}", global_id);
            return false;
        }

        global_to_local[global_id] = local_id;
        const auto& point = global_points[static_cast<size_t>(global_id)];
        const int base = static_cast<int>(local_id * 3);
        input.pointlist[base] = point[0];
        input.pointlist[base + 1] = point[1];
        input.pointlist[base + 2] = point[2];
        input.pointmarkerlist[local_id] = 0;
    }

    std::vector<Index> selected_faces = use_largest_surface_shell
        ? collectLargestSurfaceShellFaces(*mesh)
        : collectAllSurfaceFaces(*mesh);
    if (selected_faces.empty()) {
        spdlog::error("TetGenLibHandler: no surface faces selected for TetGen input");
        return false;
    }
    if (use_largest_surface_shell) {
        spdlog::info("TetGenLibHandler: selected largest surface shell with {} faces", selected_faces.size());
    }

    input.numberoffacets = static_cast<int>(selected_faces.size());
    input.facetlist = new tetgenio::facet[input.numberoffacets];
    input.facetmarkerlist = new int[input.numberoffacets];

    for (Index selected_index = 0; selected_index < static_cast<Index>(selected_faces.size()); ++selected_index) {
        const Index face_id = selected_faces[static_cast<size_t>(selected_index)];
        const Index begin = mesh->face_vertices_offset_[static_cast<size_t>(face_id)];
        const Index end = mesh->face_vertices_offset_[static_cast<size_t>(face_id + 1)];
        const Index vertex_count = end - begin;

        if (vertex_count < 3) {
            spdlog::error("TetGenLibHandler: face {} has fewer than 3 vertices", face_id);
            return false;
        }

        tetgenio::facet& facet = input.facetlist[selected_index];
        tetgenio::init(&facet);
        facet.numberofpolygons = 1;
        facet.polygonlist = new tetgenio::polygon[1];
        facet.numberofholes = 0;
        facet.holelist = nullptr;

        tetgenio::polygon& polygon = facet.polygonlist[0];
        tetgenio::init(&polygon);
        polygon.numberofvertices = static_cast<int>(vertex_count);
        polygon.vertexlist = new int[polygon.numberofvertices];

        for (Index offset = 0; offset < vertex_count; ++offset) {
            const Index stored_id = mesh->face_vertices_[static_cast<size_t>(begin + offset)];
            auto it = global_to_local.find(stored_id);
            Index local_id = stored_id;
            if (it != global_to_local.end()) {
                local_id = it->second;
            }
            if (local_id < 0 || local_id >= mesh->vertex_count_) {
                spdlog::error("TetGenLibHandler: face {} has invalid vertex id {}", face_id, stored_id);
                return false;
            }
            polygon.vertexlist[offset] = static_cast<int>(local_id);
        }

        input.facetmarkerlist[selected_index] = 0;
    }

    return true;
}

/**
 * @brief 将 TetGen tetgenio 输出转换为 PreCess MeshData
 *        处理索引偏移（TetGen 1-based 转为 PreCess 0-based）
 * @param output TetGen 输出的 tetgenio 结构
 * @return 转换后的 MeshData，失败返回 nullptr
 */
std::unique_ptr<MeshData> buildMeshDataFromTetGenOutput(const tetgenio& output)
{
    if (!output.pointlist || output.numberofpoints <= 0) {
        spdlog::error("TetGenLibHandler: TetGen output has no points");
        return nullptr;
    }
    if (!output.tetrahedronlist || output.numberoftetrahedra <= 0) {
        spdlog::error("TetGenLibHandler: TetGen output has no tetrahedra");
        return nullptr;
    }

    const int index_shift = output.firstnumber == 1 ? -1 : 0;
    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    mesh->vertex_count_ = output.numberofpoints;
    mesh->vertex_positions_.reserve(static_cast<size_t>(output.numberofpoints));
    for (int point_id = 0; point_id < output.numberofpoints; ++point_id) {
        const int base = point_id * 3;
        mesh->vertex_positions_.push_back({
            output.pointlist[base],
            output.pointlist[base + 1],
            output.pointlist[base + 2],
        });
    }

    if (output.trifacelist && output.numberoftrifaces > 0) {
        mesh->face_vertices_.reserve(static_cast<size_t>(output.numberoftrifaces * 3));
        for (int face_id = 0; face_id < output.numberoftrifaces; ++face_id) {
            const int base = face_id * 3;
            mesh->face_vertices_.push_back(output.trifacelist[base] + index_shift);
            mesh->face_vertices_.push_back(output.trifacelist[base + 1] + index_shift);
            mesh->face_vertices_.push_back(output.trifacelist[base + 2] + index_shift);
            mesh->face_vertices_offset_.push_back(static_cast<Index>(mesh->face_vertices_.size()));
        }
    }

    if (output.numberofcorners <= 0) {
        spdlog::warn("TetGenLibHandler: TetGen output has no corner info, assuming 4");
    }
    const int corners = output.numberofcorners > 0 ? output.numberofcorners : 4;
    mesh->solid_vertices_.reserve(static_cast<size_t>(output.numberoftetrahedra * 4));
    mesh->solid_types_.reserve(static_cast<size_t>(output.numberoftetrahedra));
    for (int tet_id = 0; tet_id < output.numberoftetrahedra; ++tet_id) {
        const int base = tet_id * corners;
        mesh->solid_types_.push_back(VTK_TETRA);
        mesh->solid_vertices_.push_back(output.tetrahedronlist[base] + index_shift);
        mesh->solid_vertices_.push_back(output.tetrahedronlist[base + 1] + index_shift);
        mesh->solid_vertices_.push_back(output.tetrahedronlist[base + 2] + index_shift);
        mesh->solid_vertices_.push_back(output.tetrahedronlist[base + 3] + index_shift);
        mesh->solid_vertices_offset_.push_back(static_cast<Index>(mesh->solid_vertices_.size()));
    }

    return mesh;
}
}

std::any systems::algo::TetGenLibHandler::execute(HandlerContext& context, const std::vector<core::ArgObject>& args)
{
    if (args.size() < 7) {
        spdlog::error("TetGenLibHandler: Not enough arguments provided.");
        return {};
    }

    const int* largest_shell_idx = args[0].get<ArgTypeEnum::Combo>();
    const double* quality_bound = args[1].get<ArgTypeEnum::Float>();
    const double* max_volume = args[2].get<ArgTypeEnum::Float>();
    const int* preserve_surface_idx = args[3].get<ArgTypeEnum::Combo>();
    const int* detect_intersections_idx = args[4].get<ArgTypeEnum::Combo>();
    const int* output_mode_idx = args[5].get<ArgTypeEnum::Combo>();
    const std::string* extra_switches = args[6].get<ArgTypeEnum::Text>();
    if (!largest_shell_idx || *largest_shell_idx < 0 || !quality_bound || !max_volume
        || !preserve_surface_idx || *preserve_surface_idx < 0
        || !detect_intersections_idx || *detect_intersections_idx < 0
        || !output_mode_idx || *output_mode_idx < 0 || !extra_switches) {
        spdlog::error("TetGenLibHandler: Invalid arguments.");
        return {};
    }

    ComponentData& input_component = context.cur_component.component();
    if (!input_component.mesh) {
        spdlog::error("TetGenLibHandler: current component {} has no mesh, not supported by TetGen.",
            context.cur_component.componentId());
        return {};
    }

    tetgenio input;
    tetgenio output;
    const bool use_largest_surface_shell = *largest_shell_idx == 0;
    if (!buildTetGenInput(input_component, context.cur_component.manager(), input, use_largest_surface_shell)) {
        return {};
    }

    const bool preserve_surface = *preserve_surface_idx == 0;
    const bool detect_intersections_only = *detect_intersections_idx == 0;
    std::string switches = makeTetGenSwitches(preserve_surface,
        detect_intersections_only,
        *quality_bound,
        *max_volume);
    std::string sanitized_extra_switches = sanitizeExtraTetGenSwitches(*extra_switches);
    if (!sanitized_extra_switches.empty()) {
        switches += sanitized_extra_switches;
    }
    spdlog::debug("TetGenLibHandler: tetrahedralize switches: {}", switches);

    // TetGen 内部使用全局静态变量，非线程安全，加锁保护并发调用场景
    static std::mutex s_tetgen_mutex;
    std::lock_guard<std::mutex> tetgen_lock(s_tetgen_mutex);

    try {
        tetrahedralize(switches.data(), &input, &output);
    } catch (int error_code) {
        spdlog::error("TetGenLibHandler: TetGen failed with error code {}", error_code);
        return {};
    } catch (const std::exception& e) {
        spdlog::error("TetGenLibHandler: TetGen failed: {}", e.what());
        return {};
    } catch (...) {
        spdlog::error("TetGenLibHandler: TetGen failed with unknown exception");
        return {};
    }

    if (detect_intersections_only) {
        spdlog::info("TetGenLibHandler: PLC self-intersection detection finished");
        return {};
    }

    std::unique_ptr<MeshData> output_mesh = buildMeshDataFromTetGenOutput(output);
    if (!output_mesh) {
        return {};
    }

    const bool create_new_model = *output_mode_idx == 0;
    std::string result_model_name = makeResultModelName(input_component, *quality_bound, *max_volume, preserve_surface, use_largest_surface_shell);
    if (create_new_model) {
        auto output_component = std::make_unique<ComponentData>();
        output_component->name = result_model_name;
        output_component->mesh = std::move(output_mesh);

        ComponentDatas components;
        components.push_back(std::move(output_component));
        context.cur_component.manager().addModel(result_model_name, std::move(components));
    } else if (!replaceComponentMesh(context.cur_component, std::move(output_mesh))) {
        spdlog::error("TetGenLibHandler: failed to replace mesh for component {}", context.cur_component.componentId());
        return {};
    }

    return {};
}

std::vector<core::ArgType> systems::algo::TetGenLibHandler::args_type() const
{
    return {
        core::ArgType { ArgTypeEnum::Combo, "是否仅使用最大表面壳", "是,否" },
        core::ArgType { ArgTypeEnum::Float, "质量参数 q（0表示关闭）", "1.2" },
        core::ArgType { ArgTypeEnum::Float, "最大单元体积 a（0表示关闭）", "0" },
        core::ArgType { ArgTypeEnum::Combo, "是否保留原始表面", "是,否" },
        core::ArgType { ArgTypeEnum::Combo, "是否仅检测自交", "是,否|1" },
        core::ArgType { ArgTypeEnum::Combo,
            "输出方式",
            "新建模型,替换当前模型",
            "默认新建模型；选择替换时仅替换当前 Component 的 mesh，保留 geometry 等其他数据。" },
        core::ArgType { ArgTypeEnum::Text,
            "高级 TetGen 参数",
            "",
            "追加到自动参数后，例如 O2T1e-8M；holes、regions、background mesh 等需额外数据，不能仅靠 switches 生效。" },
    };
}
