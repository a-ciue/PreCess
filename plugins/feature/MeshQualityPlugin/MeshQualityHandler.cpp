#include "MeshQualityHandler.h"

#include "ComponentOperator.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"
#include "Selection.h"
#include "SolidCellRecognition.h"

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkGenericCell.h>
#include <vtkMath.h>
#include <vtkMeshQuality.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtk_verdict.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
namespace {

    constexpr std::size_t kComponentParam = 0; //> 目标 Component 选择器参数下标
    constexpr std::size_t kMetricParam = 1; //> 质量指标参数下标

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
        std::size_t valid_count { 0 };
        double minimum { 0.0 };
        double average { 0.0 };
        double maximum { 0.0 };
    };

    /**
     * @brief 根据参数索引取得质量指标；非法索引返回空值。
     */
    std::optional<QualityMetric> metricFromIndex(int index)
    {
        switch (index) {
        case 0:
            return QualityMetric::ScaledJacobian;
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
            return std::nullopt;
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
        // MeshData 持有 double；默认 float 会使大坐标附近的小单元丢失边长。
        points->SetDataTypeToDouble();
        points->SetNumberOfPoints(static_cast<vtkIdType>(vertex_positions.size()));
        for (vtkIdType point_id = 0; point_id < static_cast<vtkIdType>(vertex_positions.size()); ++point_id) {
            points->SetPoint(point_id, vertex_positions[static_cast<std::size_t>(point_id)].data());
        }
        grid.SetPoints(points);
    }

    /** @brief 检查偏移是否完整覆盖连通性，避免截断数据或非法偏移进入 VTK。 */
    bool isValidConnectivity(const std::vector<Index>& offsets, std::size_t index_count)
    {
        if (offsets.empty())
            return index_count == 0;
        return offsets.front() == 0 && offsets.back() >= 0
            && static_cast<std::size_t>(offsets.back()) == index_count
            && std::is_sorted(offsets.begin(), offsets.end());
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
        if (!supportsFaces(metric)) {
            return { };
        }
        if (!isValidConnectivity(mesh.face_vertices_offset_, mesh.face_vertices_.size())) {
            error = "面单元顶点偏移与连通性不一致";
            return { };
        }
        if (mesh.face_vertices_offset_.size() < 2)
            return { };

        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        setLocalPoints(*grid, mesh.vertex_positions_);

        for (std::size_t face_id = 0; face_id + 1 < mesh.face_vertices_offset_.size(); ++face_id) {
            const Index begin = mesh.face_vertices_offset_[face_id];
            const Index end = mesh.face_vertices_offset_[face_id + 1];
            const Index point_count = end - begin;
            if (point_count != 3 && point_count != 4) {
                error = "面质量仅支持线性三角形和四边形单元";
                return { };
            }
            if (metric == QualityMetric::Warpage && point_count != 4) {
                error = "Warpage 仅支持纯四边形面网格";
                return { };
            }

            std::vector<vtkIdType> point_ids;
            point_ids.reserve(static_cast<std::size_t>(point_count));
            for (Index offset = begin; offset < end; ++offset) {
                const Index point_id = mesh.face_vertices_[static_cast<std::size_t>(offset)];
                if (point_id < 0 || static_cast<std::size_t>(point_id) >= mesh.vertex_positions_.size()) {
                    error = "面单元包含无效的点索引";
                    return { };
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
     * @brief 构造识别实际类型后的临时体网格，一条原单元始终对应一个计算槽位。
     */
    vtkSmartPointer<vtkUnstructuredGrid> buildSolidGrid(
        const MeshData& mesh,
        QualityMetric metric,
        std::vector<SolidCellRecord>& records,
        std::string& error)
    {
        if (!supportsSolids(metric)) {
            return { };
        }
        const std::size_t solid_count = mesh.solid_vertices_offset_.empty()
            ? 0
            : mesh.solid_vertices_offset_.size() - 1;
        if (!isValidConnectivity(mesh.solid_vertices_offset_, mesh.solid_vertices_.size())) {
            error = "体单元顶点偏移与连通性不一致";
            return { };
        }
        if (mesh.solid_types_.size() != solid_count) {
            error = "体单元类型与顶点偏移数量不一致";
            return { };
        }
        if (solid_count == 0)
            return { };

        auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
        setLocalPoints(*grid, mesh.vertex_positions_);
        records.reserve(solid_count);
        // 仅在本次构造内复用折叠模式；键首位为原类型，其余八位为节点秩（最多六面体八角点）。
        // 节点秩沿用实际编号排序，保持原识别的节点顺序。
        std::map<std::array<Index, 9>, SolidCellRecord> patterns;
        std::vector<vtkIdType> point_ids;
        point_ids.reserve(8);

        for (std::size_t solid_id = 0; solid_id < mesh.solid_types_.size(); ++solid_id) {
            const unsigned char cell_type = mesh.solid_types_[solid_id];
            const Index begin = mesh.solid_vertices_offset_[solid_id];
            const Index end = mesh.solid_vertices_offset_[solid_id + 1];
            const Index expected_count = solidPointCount(cell_type);
            if (expected_count > 0 && end - begin != expected_count) {
                error = "体单元顶点数量与单元类型不一致";
                return { };
            }

            for (Index offset = begin; offset < end; ++offset) {
                const Index point_id = mesh.solid_vertices_[static_cast<std::size_t>(offset)];
                if (point_id < 0 || static_cast<std::size_t>(point_id) >= mesh.vertex_positions_.size()) {
                    error = "体单元包含无效的点索引";
                    return { };
                }
            }
            const auto source_ids = std::span(mesh.solid_vertices_).subspan(begin, end - begin);
            std::array<Index, 8> unique_ids { };
            Index unique_count = 0;
            if (expected_count > 0) {
                std::copy(source_ids.begin(), source_ids.end(), unique_ids.begin());
                auto end_ids = unique_ids.begin() + expected_count;
                std::sort(unique_ids.begin(), end_ids);
                unique_count = static_cast<Index>(std::unique(unique_ids.begin(), end_ids) - unique_ids.begin());
            }

            SolidCellRecord record;
            if (expected_count > 0 && unique_count < expected_count) {
                std::array<Index, 9> pattern { };
                pattern[0] = cell_type;
                for (std::size_t i = 0; i < source_ids.size(); ++i)
                    pattern[i + 1] = static_cast<Index>(std::lower_bound(unique_ids.begin(),
                                                            unique_ids.begin() + unique_count, source_ids[i])
                        - unique_ids.begin());
                auto [it, inserted] = patterns.try_emplace(pattern);
                if (inserted)
                    it->second = recognizeSolidCell(0, cell_type, std::span(pattern).subspan(1, source_ids.size()));
                record = it->second;
                record.source_cell_id = static_cast<Index>(solid_id);
                for (auto& point_id : record.point_ids)
                    point_id = unique_ids[point_id];
            } else {
                record = recognizeSolidCell(static_cast<Index>(solid_id), cell_type, source_ids);
            }

            // 节点映射只参与当前单元构造；跨阶段仅保留识别诊断字段。
            const auto computation_ids = std::move(record.point_ids);
            records.push_back(std::move(record));
            const auto& diagnostic = records.back();
            if (!supportsSolidType(metric, diagnostic.evaluated_type)) {
                // 占位保留原单元编号；不让未识别/不支持的单元进入其他指标的默认计算。
                grid->InsertNextCell(VTK_EMPTY_CELL, 0, static_cast<const vtkIdType*>(nullptr));
                continue;
            }
            point_ids.assign(computation_ids.begin(), computation_ids.end());
            grid->InsertNextCell(diagnostic.evaluated_type, static_cast<vtkIdType>(point_ids.size()), point_ids.data());
        }
        return grid;
    }

    /** @brief 排除非有限坐标及重复角点，防止库把坍缩单元按其他拓扑给出正常值。 */
    bool isCellGeometryAvailable(vtkCell& cell)
    {
        if (cell.GetNumberOfPoints() == 0)
            return false;
        for (vtkIdType i = 0; i < cell.GetNumberOfPoints(); ++i) {
            double point[3];
            cell.GetPoints()->GetPoint(i, point);
            if (!std::all_of(std::begin(point), std::end(point), [](double value) { return std::isfinite(value); }))
                return false;
            for (vtkIdType j = 0; j < i; ++j) {
                double previous[3];
                cell.GetPoints()->GetPoint(j, previous);
                if (std::equal(std::begin(point), std::end(point), previous))
                    return false;
            }
        }
        return true;
    }

    /**
     * @brief 按四个顶点到对面的有向高/该面最长边计算 Collapse Ratio。
     *
     * VTK 9.6.2 的 tet_collapse_ratio 为两个面选错了边；这里按几何定义计算，
     * 保留原指标的符号与尺度（正四面体为 sqrt(2/3)，不另作归一化）。
     */
    double calculateTetCollapse(vtkCell& cell)
    {
        std::array<std::array<double, 3>, 4> points;
        double origin[3];
        cell.GetPoints()->GetPoint(0, origin);
        double scale = 0.0;
        // 先平移并统一缩放，避免叉积和长度计算随模型单位溢出或下溢。
        for (vtkIdType i = 0; i < 4; ++i) {
            cell.GetPoints()->GetPoint(i, points[i].data());
            vtkMath::Subtract(points[i].data(), origin, points[i].data());
            for (double coordinate : points[i]) {
                if (!std::isfinite(coordinate))
                    return std::numeric_limits<double>::quiet_NaN();
                scale = (std::max)(scale, std::abs(coordinate));
            }
        }
        if (scale == 0.0)
            return std::numeric_limits<double>::quiet_NaN();
        for (auto& point : points)
            for (double& coordinate : point)
                coordinate /= scale;

        // 每项的前三个下标组成有向对面，第四个下标是对应顶点。
        constexpr int faces[4][4] { { 0, 1, 2, 3 }, { 0, 3, 1, 2 }, { 0, 2, 3, 1 }, { 1, 3, 2, 0 } };
        double minimum = (std::numeric_limits<double>::max)();
        for (const auto& face : faces) {
            double edge_a[3], edge_b[3], edge_c[3], to_vertex[3], normal[3];
            vtkMath::Subtract(points[face[1]].data(), points[face[0]].data(), edge_a);
            vtkMath::Subtract(points[face[2]].data(), points[face[0]].data(), edge_b);
            vtkMath::Subtract(points[face[2]].data(), points[face[1]].data(), edge_c);
            vtkMath::Subtract(points[face[3]].data(), points[face[0]].data(), to_vertex);
            vtkMath::Cross(edge_a, edge_b, normal);
            const double area = vtkMath::Norm(normal);
            const double longest_edge = (std::max)({ vtkMath::Norm(edge_a), vtkMath::Norm(edge_b), vtkMath::Norm(edge_c) });
            if (area == 0.0 || longest_edge == 0.0)
                return std::numeric_limits<double>::quiet_NaN();
            const double ratio = (vtkMath::Dot(to_vertex, normal) / area) / longest_edge;
            minimum = (std::min)(minimum, ratio);
        }
        return minimum;
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
        vtkDataArray* array = nullptr;
        if (metric != QualityMetric::TetCollapse) {
            quality->SetInputData(&grid);
            quality->SaveCellQualityOn();
            configureMetric(*quality, metric);
            quality->Update();
            array = quality->GetOutput()->GetCellData()->GetArray("Quality");
            if (!array || array->GetNumberOfTuples() != grid.GetNumberOfCells())
                return std::nullopt;
        }

        QualityResult result;
        result.values.reserve(static_cast<std::size_t>(grid.GetNumberOfCells()));
        double sum = 0.0;
        result.minimum = (std::numeric_limits<double>::max)();
        result.maximum = std::numeric_limits<double>::lowest();
        auto cell = vtkSmartPointer<vtkGenericCell>::New();
        for (vtkIdType cell_id = 0; cell_id < grid.GetNumberOfCells(); ++cell_id) {
            grid.GetCell(cell_id, cell);
            double value = std::numeric_limits<double>::quiet_NaN();
            if (isCellGeometryAvailable(*cell))
                value = metric == QualityMetric::TetCollapse ? calculateTetCollapse(*cell) : array->GetComponent(cell_id, 0);
            // Verdict 的正负有限哨兵同样不可用；NaN 占位保持属性与原单元一一对应。
            if (!std::isfinite(value) || std::abs(value) >= verdict::VERDICT_DBL_MAX) {
                result.values.push_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }
            result.values.push_back(value);
            ++result.valid_count;
            result.minimum = (std::min)(result.minimum, value);
            result.maximum = (std::max)(result.maximum, value);
            sum += value;
        }
        if (result.values.empty()) {
            return std::nullopt;
        }
        if (result.valid_count > 0)
            result.average = sum / static_cast<double>(result.valid_count);
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
               << "，有效 " << result.valid_count
               << "，不可用 " << result.values.size() - result.valid_count;
        if (result.valid_count == 0) {
            output << "（类型未识别、指标不支持或数值不可用）\n";
            return;
        }
        output << "，最小值 " << result.minimum
               << "，平均值 " << result.average
               << "，最大值 " << result.maximum << '\n';
    }

    /** @brief 汇总实际计算类型及未识别/指标不支持数量，不把类型识别等同于质量合格。 */
    void appendRecognitionStatistics(std::ostringstream& output, std::span<const SolidCellRecord> records,
        QualityMetric metric)
    {
        std::size_t collapsed_count = 0, unrecognized_count = 0, unsupported_count = 0;
        for (const auto& record : records) {
            if (record.recognition == SolidCellRecognition::Collapsed)
                ++collapsed_count;
            if (record.recognition == SolidCellRecognition::Unsupported)
                ++unsupported_count;
            else if (record.evaluated_type == VTK_EMPTY_CELL)
                ++unrecognized_count;
            else if (!supportsSolidType(metric, record.evaluated_type))
                ++unsupported_count;
        }
        output << "体单元识别：折叠转换 " << collapsed_count << "，类型未识别 " << unrecognized_count
               << "，指标不支持 " << unsupported_count << '\n';
        for (unsigned char type : { VTK_TETRA, VTK_PYRAMID, VTK_WEDGE, VTK_HEXAHEDRON }) {
            const auto count = std::count_if(records.begin(), records.end(), [type](const auto& record) {
                return record.evaluated_type == type;
            });
            if (count == 0)
                continue;
            const char* name = type == VTK_TETRA ? "四面体" : type == VTK_PYRAMID ? "金字塔"
                : type == VTK_WEDGE                                               ? "三棱柱"
                                                                                  : "六面体";
            output << name << " " << count << '\n';
        }
    }

    /** @brief 写入可复用的自有属性，已有同名属性使用新名字避让且不纳入退出清理。 */
    std::string writeQualityAttribute(std::map<std::string, std::vector<double>>& attributes,
        std::vector<std::string>& generated_names, const std::string& base_name, const std::vector<double>& values)
    {
        std::string name = base_name + "_1";
        std::size_t suffix = 2;
        while (attributes.contains(name)
            && std::find(generated_names.begin(), generated_names.end(), name) == generated_names.end())
            name = base_name + "_" + std::to_string(suffix++) + "_1";
        attributes[name] = values;
        if (std::find(generated_names.begin(), generated_names.end(), name) == generated_names.end())
            generated_names.push_back(name);
        return name;
    }

    /** @brief 将识别记录作为与原单元一一对应的属性保存，使用相同的名称避让及退出清理规则。 */
    void writeRecognitionAttributes(MeshData& mesh, std::vector<std::string>& generated_names,
        std::span<const SolidCellRecord> records)
    {
        std::vector<double> source_ids, source_types, evaluated_types, states;
        source_ids.reserve(records.size());
        source_types.reserve(records.size());
        evaluated_types.reserve(records.size());
        states.reserve(records.size());
        for (const auto& record : records) {
            source_ids.push_back(record.source_cell_id);
            source_types.push_back(record.source_type);
            evaluated_types.push_back(record.evaluated_type);
            states.push_back(static_cast<int>(record.recognition));
        }
        writeQualityAttribute(mesh.solid_attributes_, generated_names, "s_mesh_quality_recognition_source_id", source_ids);
        writeQualityAttribute(mesh.solid_attributes_, generated_names, "s_mesh_quality_recognition_source_type", source_types);
        writeQualityAttribute(mesh.solid_attributes_, generated_names, "s_mesh_quality_recognition_evaluated_type", evaluated_types);
        writeQualityAttribute(mesh.solid_attributes_, generated_names, "s_mesh_quality_recognition_status", states);
    }

}

void MeshQualityHandler::setup(FeatureRegistrar& reg, FeatureContext&)
{
    reg.addParameter({
        ArgTypeEnum::Selector,
        "目标 Component",
        "Component",
        "选择一个需要计算网格质量的 Component",
    });
    reg.addParameter({
        ArgTypeEnum::Combo,
        "质量指标",
        "Scaled Jacobian,Equiangle Skew,Edge Ratio,最小角,最大角,Warpage,Tet Collapse|0",
        "自动识别折叠单元，保留原编号和识别记录。不可用值记为 NaN，不参与统计。Warpage 平面为 1；Tet Collapse 为有向高/对面最长边（正四面体为 sqrt(2/3)）",
    });
    reg.navigation().addEntry({ "MeshQuality", "网格质量", "qrc:/images/toolbar/Function/grid_quality.svg", 0, "功能/网格" });
}

void MeshQualityHandler::deactivate(FeatureContext& ctx)
{
    // 功能退出即清理现场：删除本次操作生成的全部质量属性（GUI 线程模型写，
    // 通知由 setFeatureActive 的操作边界 flush 统一发出）
    clearGeneratedAttributes(ctx);
}

void MeshQualityHandler::teardown(FeatureContext& ctx)
{
    // 注销兜底：退出路径（deactivate）已清理，此处仅容错空转
    clearGeneratedAttributes(ctx);
}

std::any MeshQualityHandler::execute(FeatureContext& ctx)
{
    const auto* selection_ptr = ctx.params.value(kComponentParam).get<ArgTypeEnum::Selector>();
    if (!selection_ptr || !*selection_ptr) {
        return std::string("请选择一个 Component");
    }

    const Selection& selection = **selection_ptr;
    if (selection.type != ElementEnum::Component || selection.ids.size() != 1) {
        return std::string("只能选择一个 Component");
    }

    const Index component_id = selection.ids.front();
    auto component = ctx.componentOperator(component_id);
    if (!component || !component->mesh()) {
        return std::string("所选 Component 没有网格");
    }

    const auto* metric_index = ctx.params.value(kMetricParam).get<ArgTypeEnum::Combo>();
    const auto metric_value = metric_index ? metricFromIndex(*metric_index) : std::nullopt;
    if (!metric_value)
        return std::string("无效的质量指标");
    const QualityMetric metric = *metric_value;
    const MeshData& mesh = *component->mesh();

    std::string face_error;
    std::string solid_error;
    std::vector<SolidCellRecord> solid_records;
    auto face_grid = buildFaceGrid(mesh, metric, face_error);
    auto solid_grid = buildSolidGrid(mesh, metric, solid_records, solid_error);

    std::optional<QualityResult> face_result = std::nullopt;
    std::optional<QualityResult> solid_result = std::nullopt;
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
    // 只写属性不动拓扑：NonTopology 标脏（邻接懒表不失效），通知由操作边界 flush 统一发出。
    MeshData& editable_mesh = component->editableMesh(MeshEditKind::NonTopology);
    std::string display_attribute;
    GeneratedAttributes& generated = generated_attributes_[component_id];
    if (face_result) {
        display_attribute = writeQualityAttribute(editable_mesh.face_attributes_, generated.face_names,
            "f_" + attribute_key, face_result->values);
    }
    // 面、体质量同时生成时默认显示体属性，面属性仍保留供用户手动选择。
    if (solid_result) {
        writeRecognitionAttributes(editable_mesh, generated.solid_names, solid_records);
        display_attribute = writeQualityAttribute(editable_mesh.solid_attributes_, generated.solid_names,
            "s_" + attribute_key, solid_result->values);
    }

    // Qt 桥接延迟发送显示信号，保证操作边界 flush 后再应用标量渲染。
    ctx.events.bus().publish(ScalarAttributeDisplayRequestedEvent { display_attribute, component_id });

    std::ostringstream output;
    output << std::setprecision(6) << metricDisplayName(metric) << '\n';
    if (face_result) {
        appendStatistics(output, "面单元", *face_result);
    } else if (!face_error.empty()) {
        output << "面：" << face_error << '\n';
    }
    if (solid_result) {
        appendStatistics(output, "体单元", *solid_result);
        appendRecognitionStatistics(output, solid_records, metric);
    } else if (!solid_error.empty()) {
        output << "体：" << solid_error << '\n';
    }
    return output.str();
}

void MeshQualityHandler::clearGeneratedAttributes(FeatureContext& ctx)
{
    for (const auto& [component_id, attributes] : generated_attributes_) {
        auto component = ctx.componentOperator(component_id);
        if (!component || !component->mesh())
            continue;

        MeshData& mesh = component->editableMesh(MeshEditKind::NonTopology);
        for (const std::string& name : attributes.face_names) {
            mesh.face_attributes_.erase(name);
        }
        for (const std::string& name : attributes.solid_names) {
            mesh.solid_attributes_.erase(name);
        }
    }
    generated_attributes_.clear();
}

}
