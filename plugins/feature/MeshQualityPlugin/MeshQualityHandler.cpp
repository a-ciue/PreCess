#include "MeshQualityHandler.h"

#include "ComponentOperator.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkMeshQuality.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace systems::feature {
namespace {

    /**
     * @brief 插件支持的质量指标，与参数 Combo 的选项顺序一致
     */
    enum class QualityMetric {
        ScaledJacobian,
        EquiangleSkew,
        EdgeRatio,
        MinAngle,
        MaxAngle,
        Warpage,
        TetCollapse,
    };

    /**
     * @brief 单组面或体单元的质量计算结果
     */
    struct QualityResult {
        std::vector<double> values;
        double minimum { 0.0 };
        double average { 0.0 };
        double maximum { 0.0 };
    };

    /**
     * @brief 根据参数索引取得质量指标
     */
    QualityMetric metricFromIndex(int index)
    {
        switch (index) {
        case 1:
            return QualityMetric::EquiangleSkew;
        case 2:
            return QualityMetric::EdgeRatio;
        case 3:
            return QualityMetric::MinAngle;
        case 4:
            return QualityMetric::MaxAngle;
        case 5:
            return QualityMetric::Warpage;
        case 6:
            return QualityMetric::TetCollapse;
        default:
            return QualityMetric::ScaledJacobian;
        }
    }

    /**
     * @brief 返回指标对应的稳定属性名
     */
    const char* metricKey(QualityMetric metric)
    {
        switch (metric) {
        case QualityMetric::EquiangleSkew:
            return "mesh_quality_equiangle_skew";
        case QualityMetric::EdgeRatio:
            return "mesh_quality_edge_ratio";
        case QualityMetric::MinAngle:
            return "mesh_quality_min_angle";
        case QualityMetric::MaxAngle:
            return "mesh_quality_max_angle";
        case QualityMetric::Warpage:
            return "mesh_quality_warpage";
        case QualityMetric::TetCollapse:
            return "mesh_quality_tet_collapse";
        default:
            return "mesh_quality_scaled_jacobian";
        }
    }

    /**
     * @brief 返回结果区使用的指标名称
     */
    const char* metricDisplayName(QualityMetric metric)
    {
        switch (metric) {
        case QualityMetric::EquiangleSkew:
            return "Equiangle Skew";
        case QualityMetric::EdgeRatio:
            return "Edge Ratio";
        case QualityMetric::MinAngle:
            return "最小角";
        case QualityMetric::MaxAngle:
            return "最大角";
        case QualityMetric::Warpage:
            return "Warpage";
        case QualityMetric::TetCollapse:
            return "Tet Collapse";
        default:
            return "Scaled Jacobian";
        }
    }

    /**
     * @brief 判断指标是否需要计算面单元
     */
    bool supportsFaces(QualityMetric metric)
    {
        return metric != QualityMetric::TetCollapse;
    }

    /**
     * @brief 判断指标是否需要计算体单元
     */
    bool supportsSolids(QualityMetric metric)
    {
        return metric == QualityMetric::ScaledJacobian
            || metric == QualityMetric::EquiangleSkew
            || metric == QualityMetric::EdgeRatio
            || metric == QualityMetric::TetCollapse;
    }

    /**
     * @brief 将组件网格的顶点坐标挂到 VTK 网格
     */
    void setLocalPoints(
        vtkUnstructuredGrid& grid,
        const std::vector<std::array<double, 3>>& vertex_positions)
    {
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetNumberOfPoints(static_cast<vtkIdType>(vertex_positions.size()));
        for (vtkIdType point_id = 0; point_id < static_cast<vtkIdType>(vertex_positions.size()); ++point_id) {
            points->SetPoint(point_id, vertex_positions[static_cast<std::size_t>(point_id)].data());
        }
        grid.SetPoints(points);
    }

    /**
     * @brief 构造待计算的三角形、四边形面网格
     *
     * Warpage 仅接受纯四边形，其余面指标接受三角形和四边形。
     */
    vtkSmartPointer<vtkUnstructuredGrid> buildFaceGrid(
        const MeshData& mesh,
        QualityMetric metric,
        std::string& error)
    {
        if (!supportsFaces(metric) || mesh.face_vertices_offset_.size() < 2) {
            return {};
        }

        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        setLocalPoints(*grid, mesh.vertex_positions_);

        for (std::size_t face_id = 0; face_id + 1 < mesh.face_vertices_offset_.size(); ++face_id) {
            const Index begin = mesh.face_vertices_offset_[face_id];
            const Index end = mesh.face_vertices_offset_[face_id + 1];
            const Index point_count = end - begin;
            if (begin < 0 || end < begin || end > static_cast<Index>(mesh.face_vertices_.size())
                || (point_count != 3 && point_count != 4)) {
                error = "面质量仅支持线性三角形和四边形单元";
                return {};
            }
            if (metric == QualityMetric::Warpage && point_count != 4) {
                error = "Warpage 仅支持纯四边形面网格";
                return {};
            }

            std::vector<vtkIdType> point_ids;
            point_ids.reserve(static_cast<std::size_t>(point_count));
            for (Index offset = begin; offset < end; ++offset) {
                const Index point_id = mesh.face_vertices_[static_cast<std::size_t>(offset)];
                if (point_id < 0 || point_id >= mesh.vertex_count_) {
                    error = "面单元包含无效的点索引";
                    return {};
                }
                point_ids.push_back(static_cast<vtkIdType>(point_id));
            }
            const int cell_type = point_count == 3 ? VTK_TRIANGLE : VTK_QUAD;
            grid->InsertNextCell(cell_type, static_cast<vtkIdType>(point_ids.size()), point_ids.data());
        }
        return grid;
    }

    /**
     * @brief 判断体单元是否支持当前质量指标
     */
    bool supportsSolidType(QualityMetric metric, unsigned char cell_type)
    {
        const bool standard_type = cell_type == VTK_TETRA || cell_type == VTK_PYRAMID
            || cell_type == VTK_WEDGE || cell_type == VTK_HEXAHEDRON;
        if (!standard_type) {
            return false;
        }
        if (metric == QualityMetric::TetCollapse) {
            return cell_type == VTK_TETRA;
        }
        if (metric == QualityMetric::EdgeRatio) {
            return cell_type != VTK_PYRAMID;
        }
        return metric == QualityMetric::ScaledJacobian
            || metric == QualityMetric::EquiangleSkew;
    }

    /**
     * @brief 返回线性标准体单元应有的顶点数
     */
    Index solidPointCount(unsigned char cell_type)
    {
        switch (cell_type) {
        case VTK_TETRA:
            return 4;
        case VTK_PYRAMID:
            return 5;
        case VTK_WEDGE:
            return 6;
        case VTK_HEXAHEDRON:
            return 8;
        default:
            return 0;
        }
    }

    /**
     * @brief 构造待计算的标准线性体网格
     */
    vtkSmartPointer<vtkUnstructuredGrid> buildSolidGrid(
        const MeshData& mesh,
        QualityMetric metric,
        std::string& error)
    {
        if (!supportsSolids(metric) || mesh.solid_vertices_offset_.size() < 2) {
            return {};
        }
        if (mesh.solid_types_.size() + 1 != mesh.solid_vertices_offset_.size()) {
            error = "体单元类型与顶点偏移数量不一致";
            return {};
        }

        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        setLocalPoints(*grid, mesh.vertex_positions_);

        for (std::size_t solid_id = 0; solid_id < mesh.solid_types_.size(); ++solid_id) {
            const unsigned char cell_type = mesh.solid_types_[solid_id];
            const Index begin = mesh.solid_vertices_offset_[solid_id];
            const Index end = mesh.solid_vertices_offset_[solid_id + 1];
            if (!supportsSolidType(metric, cell_type)) {
                error = std::string(metricDisplayName(metric)) + " 不支持当前 Component 中的体单元类型";
                return {};
            }
            if (begin < 0 || end < begin || end > static_cast<Index>(mesh.solid_vertices_.size())
                || end - begin != solidPointCount(cell_type)) {
                error = "体单元顶点数量与单元类型不一致";
                return {};
            }

            std::vector<vtkIdType> point_ids;
            point_ids.reserve(static_cast<std::size_t>(end - begin));
            for (Index offset = begin; offset < end; ++offset) {
                const Index point_id = mesh.solid_vertices_[static_cast<std::size_t>(offset)];
                if (point_id < 0 || point_id >= mesh.vertex_count_) {
                    error = "体单元包含无效的点索引";
                    return {};
                }
                point_ids.push_back(static_cast<vtkIdType>(point_id));
            }
            grid->InsertNextCell(cell_type, static_cast<vtkIdType>(point_ids.size()), point_ids.data());
        }
        return grid;
    }

    /**
     * @brief 为 VTK 质量过滤器配置面、体单元的指标
     */
    void configureMetric(vtkMeshQuality& quality, QualityMetric metric)
    {
        switch (metric) {
        case QualityMetric::EquiangleSkew:
            quality.SetTriangleQualityMeasureToEquiangleSkew();
            quality.SetQuadQualityMeasureToEquiangleSkew();
            quality.SetTetQualityMeasureToEquiangleSkew();
            quality.SetPyramidQualityMeasureToEquiangleSkew();
            quality.SetWedgeQualityMeasureToEquiangleSkew();
            quality.SetHexQualityMeasureToEquiangleSkew();
            break;
        case QualityMetric::EdgeRatio:
            quality.SetTriangleQualityMeasureToEdgeRatio();
            quality.SetQuadQualityMeasureToEdgeRatio();
            quality.SetTetQualityMeasureToEdgeRatio();
            quality.SetWedgeQualityMeasureToEdgeRatio();
            quality.SetHexQualityMeasureToEdgeRatio();
            break;
        case QualityMetric::MinAngle:
            quality.SetTriangleQualityMeasureToMinAngle();
            quality.SetQuadQualityMeasureToMinAngle();
            break;
        case QualityMetric::MaxAngle:
            quality.SetTriangleQualityMeasureToMaxAngle();
            quality.SetQuadQualityMeasureToMaxAngle();
            break;
        case QualityMetric::Warpage:
            quality.SetQuadQualityMeasureToWarpage();
            break;
        case QualityMetric::TetCollapse:
            quality.SetTetQualityMeasureToCollapseRatio();
            break;
        default:
            quality.SetTriangleQualityMeasureToScaledJacobian();
            quality.SetQuadQualityMeasureToScaledJacobian();
            quality.SetTetQualityMeasureToScaledJacobian();
            quality.SetPyramidQualityMeasureToScaledJacobian();
            quality.SetWedgeQualityMeasureToScaledJacobian();
            quality.SetHexQualityMeasureToScaledJacobian();
            break;
        }
    }

    /**
     * @brief 运行 VTK 质量过滤器并提取逐单元数值及统计量
     */
    std::optional<QualityResult> calculateQuality(vtkUnstructuredGrid& grid, QualityMetric metric)
    {
        auto quality = vtkSmartPointer<vtkMeshQuality>::New();
        quality->SetInputData(&grid);
        quality->SaveCellQualityOn();
        configureMetric(*quality, metric);
        quality->Update();

        vtkDataArray* array = quality->GetOutput()->GetCellData()->GetArray("Quality");
        if (!array || array->GetNumberOfTuples() != grid.GetNumberOfCells()) {
            return std::nullopt;
        }

        QualityResult result;
        result.values.reserve(static_cast<std::size_t>(array->GetNumberOfTuples()));
        double sum = 0.0;
        result.minimum = std::numeric_limits<double>::max();
        result.maximum = std::numeric_limits<double>::lowest();
        for (vtkIdType cell_id = 0; cell_id < array->GetNumberOfTuples(); ++cell_id) {
            const double value = array->GetComponent(cell_id, 0);
            if (!std::isfinite(value)) {
                return std::nullopt;
            }
            result.values.push_back(value);
            result.minimum = std::min(result.minimum, value);
            result.maximum = std::max(result.maximum, value);
            sum += value;
        }
        if (result.values.empty()) {
            return std::nullopt;
        }
        result.average = sum / static_cast<double>(result.values.size());
        return result;
    }

    /**
     * @brief 将一组质量统计追加到功能执行结果
     */
    void appendStatistics(
        std::ostringstream& output,
        const char* entity_name,
        const QualityResult& result)
    {
        output << entity_name << "：数量 " << result.values.size()
               << "，最小值 " << result.minimum
               << "，平均值 " << result.average
               << "，最大值 " << result.maximum << '\n';
    }

}

void MeshQualityHandler::setup(FeatureRegistrar& reg)
{
    reg.addParameter({
        ArgTypeEnum::Combo,
        "质量指标",
        "Scaled Jacobian,Equiangle Skew,Edge Ratio,最小角,最大角,Warpage,Tet Collapse|0",
        "选择要计算并写入面、体属性的网格质量指标",
    });
    reg.addMenuItem({ "功能/网格", "网格质量" });
}

void MeshQualityHandler::activate(FeatureContext& ctx)
{
    attribute_display_sub_ = ctx.events.subscribe<ScalarAttributeDisplayRequestedEvent>(
        [this, context = &ctx](const ScalarAttributeDisplayRequestedEvent& event) {
            // 空属性名表示活动操作已切换，清理本次生成的全部质量属性。
            if (event.attribute_name.empty())
                clearGeneratedAttributes(*context);
        });
}

std::any MeshQualityHandler::execute(FeatureContext& ctx)
{
    const auto component_id = ctx.activeComponent ? ctx.activeComponent() : std::nullopt;
    if (!component_id) {
        return std::string("未选择 Component");
    }
    auto component = ctx.componentOperator ? ctx.componentOperator(*component_id) : std::nullopt;
    if (!component || !component->mesh()) {
        return std::string("当前 Component 没有网格");
    }

    int metric_index = 0;
    if (const auto* value = ctx.params.value(0).get<ArgTypeEnum::Combo>()) {
        metric_index = *value;
    }
    const QualityMetric metric = metricFromIndex(metric_index);
    MeshData& mesh = *component->mesh();

    std::string face_error;
    std::string solid_error;
    auto face_grid = buildFaceGrid(mesh, metric, face_error);
    auto solid_grid = buildSolidGrid(mesh, metric, solid_error);

    std::optional<QualityResult> face_result;
    std::optional<QualityResult> solid_result;
    if (face_grid && face_grid->GetNumberOfCells() > 0) {
        face_result = calculateQuality(*face_grid, metric);
        if (!face_result) {
            face_error = "面质量计算失败";
        }
    }
    if (solid_grid && solid_grid->GetNumberOfCells() > 0) {
        solid_result = calculateQuality(*solid_grid, metric);
        if (!solid_result) {
            solid_error = "体质量计算失败";
        }
    }

    if (!face_result && !solid_result) {
        std::ostringstream error;
        error << metricDisplayName(metric) << " 没有可计算的单元";
        if (!face_error.empty()) {
            error << "\n面：" << face_error;
        }
        if (!solid_error.empty()) {
            error << "\n体：" << solid_error;
        }
        return error.str();
    }

    const std::string attribute_key = metricKey(metric);
    std::string display_attribute;
    GeneratedAttributes& generated = generated_attributes_[*component_id];
    if (face_result) {
        const std::string face_attribute = "f_" + attribute_key + "_1";
        mesh.face_attributes_[face_attribute] = face_result->values;
        generated.face_names.push_back(face_attribute);
        display_attribute = face_attribute;
    }
    // 面、体质量同时生成时默认显示体属性，面属性仍保留供用户手动选择。
    if (solid_result) {
        const std::string solid_attribute = "s_" + attribute_key + "_1";
        mesh.solid_attributes_[solid_attribute] = solid_result->values;
        generated.solid_names.push_back(solid_attribute);
        display_attribute = solid_attribute;
    }

    // 先刷新模型观察者，再请求 app 层显示已写入的标量属性。
    component->notifyChanged();
    ctx.events.publish(ScalarAttributeDisplayRequestedEvent { display_attribute });

    std::ostringstream output;
    output << std::setprecision(6) << metricDisplayName(metric) << '\n';
    if (face_result) {
        appendStatistics(output, "面单元", *face_result);
    } else if (!face_error.empty()) {
        output << "面：" << face_error << '\n';
    }
    if (solid_result) {
        appendStatistics(output, "体单元", *solid_result);
    } else if (!solid_error.empty()) {
        output << "体：" << solid_error << '\n';
    }
    return output.str();
}

void MeshQualityHandler::clearGeneratedAttributes(FeatureContext& ctx)
{
    for (const auto& [component_id, attributes] : generated_attributes_) {
        auto component = ctx.componentOperator ? ctx.componentOperator(component_id) : std::nullopt;
        if (!component || !component->mesh())
            continue;

        MeshData& mesh = *component->mesh();
        bool changed = false;
        for (const std::string& name : attributes.face_names) {
            changed = mesh.face_attributes_.erase(name) > 0 || changed;
        }
        for (const std::string& name : attributes.solid_names) {
            changed = mesh.solid_attributes_.erase(name) > 0 || changed;
        }
        if (changed)
            component->notifyChanged();
    }
    generated_attributes_.clear();
}

}
