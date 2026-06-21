#include "TetGenLibHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <array>
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "tetgen.h"

namespace {
constexpr unsigned char kVtkTetra = 10;

std::string makeTetGenSwitches(bool keep_outer,
    bool preserve_surface,
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
    if (keep_outer) {
        switches += "H";
    }
    switches += "Q";
    return switches;
}

bool buildTetGenInput(const ComponentData& component, const ModelLayer& manager, tetgenio& input)
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

    const Index face_count = static_cast<Index>(mesh->face_vertices_offset_.size() - 1);
    input.numberoffacets = static_cast<int>(face_count);
    input.facetlist = new tetgenio::facet[input.numberoffacets];
    input.facetmarkerlist = new int[input.numberoffacets];

    for (Index face_id = 0; face_id < face_count; ++face_id) {
        const Index begin = mesh->face_vertices_offset_[static_cast<size_t>(face_id)];
        const Index end = mesh->face_vertices_offset_[static_cast<size_t>(face_id + 1)];
        const Index vertex_count = end - begin;

        if (vertex_count < 3) {
            spdlog::error("TetGenLibHandler: face {} has fewer than 3 vertices", face_id);
            return false;
        }

        tetgenio::facet& facet = input.facetlist[face_id];
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

        input.facetmarkerlist[face_id] = 0;
    }

    return true;
}

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

    const int corners = output.numberofcorners > 0 ? output.numberofcorners : 4;
    mesh->solid_vertices_.reserve(static_cast<size_t>(output.numberoftetrahedra * 4));
    mesh->solid_types_.reserve(static_cast<size_t>(output.numberoftetrahedra));
    for (int tet_id = 0; tet_id < output.numberoftetrahedra; ++tet_id) {
        const int base = tet_id * corners;
        mesh->solid_types_.push_back(kVtkTetra);
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
    if (args.size() < 5) {
        spdlog::critical("TetGenLibHandler: Not enough arguments provided.");
        return {};
    }

    const int* keep_outer_idx = args[0].get<ArgTypeEnum::Combo>();
    const double* quality_bound = args[1].get<ArgTypeEnum::Float>();
    const double* max_volume = args[2].get<ArgTypeEnum::Float>();
    const int* preserve_surface_idx = args[3].get<ArgTypeEnum::Combo>();
    const int* detect_intersections_idx = args[4].get<ArgTypeEnum::Combo>();
    if (!keep_outer_idx || *keep_outer_idx < 0 || !quality_bound || !max_volume
        || !preserve_surface_idx || *preserve_surface_idx < 0
        || !detect_intersections_idx || *detect_intersections_idx < 0) {
        spdlog::critical("TetGenLibHandler: Invalid arguments.");
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
    if (!buildTetGenInput(input_component, context.cur_component.manager(), input)) {
        return {};
    }

    const bool keep_outer = *keep_outer_idx == 0;
    const bool preserve_surface = *preserve_surface_idx == 0;
    const bool detect_intersections_only = *detect_intersections_idx == 0;
    std::string switches = makeTetGenSwitches(keep_outer,
        preserve_surface,
        detect_intersections_only,
        *quality_bound,
        *max_volume);
    spdlog::debug("TetGenLibHandler: tetrahedralize switches: {}", switches);

    try {
        tetrahedralize(switches.data(), &input, &output);
    } catch (int error_code) {
        spdlog::critical("TetGenLibHandler: TetGen failed with error code {}", error_code);
        return {};
    } catch (const std::exception& e) {
        spdlog::critical("TetGenLibHandler: TetGen failed: {}", e.what());
        return {};
    } catch (...) {
        spdlog::critical("TetGenLibHandler: TetGen failed with unknown exception");
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

    auto output_component = std::make_unique<ComponentData>();
    output_component->name = input_component.name.empty() ? "TetGen Result" : input_component.name + " TetGen";
    output_component->mesh = std::move(output_mesh);

    ComponentDatas components;
    components.push_back(std::move(output_component));
    context.cur_component.manager().addModel("TetGen Result", std::move(components));

    return {};
}

std::vector<core::ArgType> systems::algo::TetGenLibHandler::args_type() const
{
    return {
        core::ArgType { ArgTypeEnum::Combo, "是否仅保留最外层腔体", "是,否|1" },
        core::ArgType { ArgTypeEnum::Float, "质量参数 q（0表示关闭）", "1.2" },
        core::ArgType { ArgTypeEnum::Float, "最大单元体积 a（0表示关闭）", "0" },
        core::ArgType { ArgTypeEnum::Combo, "是否保留原始表面", "是,否|1" },
        core::ArgType { ArgTypeEnum::Combo, "是否仅检测自交", "是,否|1" },
    };
}
