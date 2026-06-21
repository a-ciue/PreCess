#pragma once

// Gmsh 曲面网格类型。数值需要和前端 Combo 的选项顺序保持一致。
enum class GmshSurfaceMeshType {
    Triangle = 0,
    QuadDominant = 1,
    StructuredQuadrilateral = 2
};
