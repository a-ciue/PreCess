#pragma once

#include <array>
#include <cstddef>

// Gmsh 曲面网格类型。数值需要和前端“网格类型” Combo 的选项顺序一致。
enum class GmshSurfaceMeshType {
    Triangle = 0,
    QuadDominant = 1,
    StructuredQuadrilateral = 2
};

// Gmsh 二维网格算法，对应 Mesh.Algorithm 的真实取值。
enum class GmshMeshAlgorithm {
    MeshAdapt = 1,
    Automatic = 2,
    Delaunay = 5,
    FrontalDelaunay = 6,
    Bamg = 7,
    FrontalDelaunayForQuads = 8,
    PackingParallelograms = 9,
    QuasiStructuredQuad = 11
};

// Gmsh 四边形重组算法，对应 Mesh.RecombinationAlgorithm 的真实取值。
enum class GmshRecombinationAlgorithm {
    UseGmshDefault = -1,
    Simple = 0,
    Blossom = 1,
    SimpleFullQuad = 2,
    BlossomFullQuad = 3
};

// 前端 Combo 文案和下面的取值数组必须保持同一顺序。
inline constexpr const char* kGmshSurfaceMeshTypeComboText =
    "三角形,四边形主导,结构化四边形";

inline constexpr const char* kGmshMeshAlgorithmComboText =
    "默认(Frontal-Delaunay),MeshAdapt,Automatic,Delaunay,"
    "BAMG,Frontal-Delaunay for Quads,"
    "Packing Parallelograms,Quasi-structured Quad";

inline constexpr std::array<int, 9> kGmshMeshAlgorithmComboValues {
    static_cast<int>(GmshMeshAlgorithm::FrontalDelaunay),
    static_cast<int>(GmshMeshAlgorithm::MeshAdapt),
    static_cast<int>(GmshMeshAlgorithm::Automatic),
    static_cast<int>(GmshMeshAlgorithm::Delaunay),
    static_cast<int>(GmshMeshAlgorithm::Bamg),
    static_cast<int>(GmshMeshAlgorithm::FrontalDelaunayForQuads),
    static_cast<int>(GmshMeshAlgorithm::PackingParallelograms),
    static_cast<int>(GmshMeshAlgorithm::QuasiStructuredQuad)
};

inline constexpr const char* kGmshRecombinationAlgorithmComboText =
    "默认,Simple,Blossom,Simple full-quad,Blossom full-quad";

inline constexpr std::array<int, 5> kGmshRecombinationAlgorithmComboValues {
    static_cast<int>(GmshRecombinationAlgorithm::UseGmshDefault),
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
