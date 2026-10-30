#include "SolidCellRecognition.h"

#include <vtkCellType.h>
#include <vtkGenericCell.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPoints.h>

#include <algorithm>
#include <utility>

namespace systems::feature {
namespace {

    using FaceRings = std::vector<std::vector<Index>>;

    /** @brief 从 VTK 标准单元取得有向面环，避免维护另一套节点顺序约定。 */
    FaceRings referenceFaces(unsigned char cell_type)
    {
        vtkNew<vtkGenericCell> cell;
        cell->SetCellType(cell_type);
        const Index count = solidPointCount(cell_type);
        cell->GetPointIds()->SetNumberOfIds(count);
        cell->GetPoints()->SetNumberOfPoints(count);
        for (Index i = 0; i < count; ++i) {
            cell->GetPointIds()->SetId(i, i);
            cell->GetPoints()->SetPoint(i, 0.0, 0.0, 0.0);
        }
        FaceRings faces;
        for (int i = 0; i < cell->GetNumberOfFaces(); ++i) {
            vtkCell* face = cell->GetFace(i);
            std::vector<Index> ring;
            for (vtkIdType j = 0; j < face->GetNumberOfPoints(); ++j)
                ring.push_back(static_cast<Index>(face->GetPointId(j)));
            faces.push_back(std::move(ring));
        }
        return faces;
    }

    /** @brief 缓存四种标准体单元的面环；返回值发布后只读。 */
    const FaceRings& cellFaces(unsigned char cell_type)
    {
        static const FaceRings tet = referenceFaces(VTK_TETRA);
        static const FaceRings pyramid = referenceFaces(VTK_PYRAMID);
        static const FaceRings wedge = referenceFaces(VTK_WEDGE);
        static const FaceRings hex = referenceFaces(VTK_HEXAHEDRON);
        switch (cell_type) {
        case VTK_TETRA:
            return tet;
        case VTK_PYRAMID:
            return pyramid;
        case VTK_WEDGE:
            return wedge;
        default:
            return hex;
        }
    }

    /** @brief 以循环移位规范化面环并排序；不反转面环，保留原单元的朝向。 */
    void canonicalizeFaces(FaceRings& faces)
    {
        for (auto& ring : faces)
            std::rotate(ring.begin(), std::min_element(ring.begin(), ring.end()), ring.end());
        std::sort(faces.begin(), faces.end());
    }

    /** @brief 将标准面环映射为实际节点；只消去连续重复角点及降为边/点的面。 */
    bool collapseFaces(const FaceRings& source, std::span<const Index> point_ids, FaceRings& collapsed)
    {
        for (const auto& face : source) {
            std::vector<Index> ring;
            for (Index position : face) {
                const Index id = point_ids[position];
                if (ring.empty() || ring.back() != id)
                    ring.push_back(id);
            }
            if (ring.size() > 1 && ring.front() == ring.back())
                ring.pop_back();
            if (ring.size() < 3)
                continue;
            auto unique = ring;
            std::sort(unique.begin(), unique.end());
            // 非相邻重复表示面环自粘，不能按普通三角形/四边形解释。
            if (std::adjacent_find(unique.begin(), unique.end()) != unique.end())
                return false;
            collapsed.push_back(std::move(ring));
        }
        canonicalizeFaces(collapsed);
        return true;
    }

}

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

SolidCellRecord recognizeSolidCell(Index source_cell_id, unsigned char source_type, std::span<const Index> point_ids)
{
    SolidCellRecord record;
    record.source_cell_id = source_cell_id;
    record.source_type = source_type;
    const Index expected_count = solidPointCount(source_type);
    if (expected_count == 0) {
        record.recognition = SolidCellRecognition::Unsupported;
        return record;
    }
    if (point_ids.size() != static_cast<std::size_t>(expected_count)
        || std::any_of(point_ids.begin(), point_ids.end(), [](Index id) { return id < 0; }))
        return record;

    std::vector<Index> unique(point_ids.begin(), point_ids.end());
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    if (unique.size() == point_ids.size()) {
        record.evaluated_type = source_type;
        record.recognition = SolidCellRecognition::Original;
        record.point_ids.assign(point_ids.begin(), point_ids.end());
        return record;
    }

    unsigned char candidate = VTK_EMPTY_CELL;
    switch (unique.size()) {
    case 4:
        candidate = VTK_TETRA;
        break;
    case 5:
        candidate = VTK_PYRAMID;
        break;
    case 6:
        candidate = VTK_WEDGE;
        break;
    default:
        return record;
    }
    FaceRings collapsed;
    if (!collapseFaces(cellFaces(source_type), point_ids, collapsed)
        || collapsed.size() != cellFaces(candidate).size())
        return record;

    // 角点数只用于缩小候选范围；完整有向面集合匹配成功才确认类型。
    // 枚举最多 6! 个映射，仅折叠单元走此路径；排序保证识别记录可复现。
    do {
        FaceRings mapped;
        if (collapseFaces(cellFaces(candidate), unique, mapped) && mapped == collapsed) {
            record.evaluated_type = candidate;
            record.recognition = SolidCellRecognition::Collapsed;
            record.point_ids = std::move(unique);
            return record;
        }
    } while (std::next_permutation(unique.begin(), unique.end()));
    return record;
}

}
