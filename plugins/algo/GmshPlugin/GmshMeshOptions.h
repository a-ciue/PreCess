#pragma once

#include <array>
#include <cstddef>

// Gmsh 曲面网格类型。数值需要和前端Combo 的选项顺序一致。
enum class GmshSurfaceMeshType {
    // 0：普通三角形网格，不启用四边形重组或结构化约束。
    Triangle = 0,
    // 1：四边形主导网格，先生成二维网格，再通过 Recombine 尽量重组成四边形。
    QuadDominant = 1,
    // 2：结构化四边形网格，使用 Transfinite Curve/Surface 约束生成行列结构。
    StructuredQuadrilateral = 2
};

// Gmsh 二维曲面网格算法，对应 Mesh.Algorithm 的官方取值。
enum class GmshMeshAlgorithm {
    // 1：MeshAdapt。基于局部网格修改，复杂曲面上通常更鲁棒。
    MeshAdapt = 1,
    // 2：Automatic。Gmsh 自动选择；平面通常用 Delaunay，非平面通常用 MeshAdapt。
    Automatic = 2,
    // 3：Initial mesh only。只生成初始网格，主要用于调试，暂不暴露到 UI。
    InitialMeshOnly = 3,
    // 5：Delaunay。平面大网格速度较快，对复杂尺寸场也比较适合。
    Delaunay = 5,
    // 6：Frontal-Delaunay。Gmsh 二维算法默认值，通常能得到较好的三角形质量。
    FrontalDelaunay = 6,
    // 7：BAMG。偏各向异性三角网格；当前插件没有专门的各向异性参数。
    Bamg = 7,
    // 8：Frontal-Delaunay for Quads。生成更适合后续四边形重组的三角网格。
    FrontalDelaunayForQuads = 8,
    // 9：Packing of Parallelograms。四边形相关算法，适用范围比默认算法更窄。
    PackingParallelograms = 9,
    // 11：Quasi-structured Quad。准结构化四边形算法；暂不暴露到 UI，避免和结构化四边形混淆。
    QuasiStructuredQuad = 11
};

// Gmsh 四边形重组算法，对应 Mesh.RecombinationAlgorithm 的真实取值。
enum class GmshRecombinationAlgorithm {
    // 0：Simple。简单重组算法。
    Simple = 0,
    // 1：Blossom。Gmsh 默认重组算法，通常质量和成功率更均衡。
    Blossom = 1,
    // 2：Simple full-quad。尝试生成全四边形网格。
    SimpleFullQuad = 2,
    // 3：Blossom full-quad。基于 Blossom 的全四边形重组。
    BlossomFullQuad = 3
};

// 前端 Combo 文案和下面的取值数组必须保持同一顺序。
inline constexpr const char* kGmshSurfaceMeshTypeComboText =
    "三角形,四边形主导,结构化四边形";

inline constexpr const char* kGmshMeshAlgorithmComboText =
    "默认(Frontal-Delaunay),MeshAdapt,Automatic,Delaunay,"
    "Frontal-Delaunay,BAMG,Frontal-Delaunay for Quads,"
    "Packing Parallelograms";

inline constexpr std::array<int, 8> kGmshMeshAlgorithmComboValues {
    static_cast<int>(GmshMeshAlgorithm::FrontalDelaunay),
    static_cast<int>(GmshMeshAlgorithm::MeshAdapt),
    static_cast<int>(GmshMeshAlgorithm::Automatic),
    static_cast<int>(GmshMeshAlgorithm::Delaunay),
    static_cast<int>(GmshMeshAlgorithm::FrontalDelaunay),
    static_cast<int>(GmshMeshAlgorithm::Bamg),
    static_cast<int>(GmshMeshAlgorithm::FrontalDelaunayForQuads),
    static_cast<int>(GmshMeshAlgorithm::PackingParallelograms)
};

inline constexpr const char* kGmshRecombinationAlgorithmComboText =
    "默认(Blossom),Simple,Blossom,Simple full-quad,Blossom full-quad";

inline constexpr std::array<int, 5> kGmshRecombinationAlgorithmComboValues {
    static_cast<int>(GmshRecombinationAlgorithm::Blossom),
    static_cast<int>(GmshRecombinationAlgorithm::Simple),
    static_cast<int>(GmshRecombinationAlgorithm::Blossom),
    static_cast<int>(GmshRecombinationAlgorithm::SimpleFullQuad),
    static_cast<int>(GmshRecombinationAlgorithm::BlossomFullQuad)
};

// 将 Combo 索引转换为 Gmsh option 的真实取值；索引异常时使用第 0 项默认值。
template <std::size_t N>
int gmshComboValue(const std::array<int, N>& values, int comboIndex)
{
    if (comboIndex < 0)
        return values[0];

    std::size_t index = static_cast<std::size_t>(comboIndex);
    if (index >= values.size())
        return values[0];

    return values[index];
}
