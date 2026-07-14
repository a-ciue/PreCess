#include "gmsh.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <vector>

int main()
{
    if (false) {
        // 定义矩形的四个角点
        double lc = 0.1; // 网格特征尺寸
        int p1 = gmsh::model::occ::addPoint(0, 0, 0, lc);
        int p2 = gmsh::model::occ::addPoint(1, 0, 0, lc);
        int p3 = gmsh::model::occ::addPoint(1, 1, 0, lc);
        int p4 = gmsh::model::occ::addPoint(0, 1, 0, lc);

        // 定义四条边
        int l1 = gmsh::model::occ::addLine(p1, p2);
        int l2 = gmsh::model::occ::addLine(p2, p3);
        int l3 = gmsh::model::occ::addLine(p3, p4);
        int l4 = gmsh::model::occ::addLine(p4, p1);

        // 创建曲线环
        int loop = gmsh::model::occ::addCurveLoop({ l1, l2, l3, l4 });

        // 创建平面曲面
        int surface = gmsh::model::occ::addPlaneSurface({ loop });

        int p5 = gmsh::model::occ::addPoint(0, 0, 0, lc);
        int p6 = gmsh::model::occ::addPoint(1, 0, 0, lc);
        int p7 = gmsh::model::occ::addPoint(1, 1, 0, lc);
        int p8 = gmsh::model::occ::addPoint(0, 1, 0, lc);

        // 定义四条边
        int l5 = gmsh::model::occ::addLine(p5, p6);
        int l6 = gmsh::model::occ::addLine(p6, p7);
        int l7 = gmsh::model::occ::addLine(p7, p8);
        int l8 = gmsh::model::occ::addLine(p8, p5);

        gmsh::model::occ::addPoint(0, 0, 0, lc);
        gmsh::model::occ::addPoint(1, 0, 0, lc);
        gmsh::model::occ::addPoint(1, 1, 0, lc);
        gmsh::model::occ::addPoint(0, 1, 0, lc);

        int s1 = gmsh::model::occ::addRectangle(0, 0, 0, 10, 10);
    }
    // 初始化 Gmsh
    gmsh::initialize();
    gmsh::option::setNumber("General.Terminal", 1);
    gmsh::model::add("test_rectangle");

    std::vector<std::pair<int, int>> outDimTags;
    std::vector<std::vector<std::pair<int, int>>> outDimTagsMap;
    gmsh::model::occ::importShapes("E:/VSProject/_models/models/step_boundary_colors.stp", outDimTags);
    gmsh::model::occ::synchronize();

    gmsh::option::setNumber("Mesh.MeshOnlyEmpty", 1);
    gmsh::option::setNumber("Mesh.MeshOnlyVisible", 1);
    gmsh::option::setNumber("Mesh.SaveAll", 1);

    // 禁用从顶点继承尺寸，完全由Field控制
    gmsh::option::setNumber("Mesh.MeshSizeFromPoints", 0);
    gmsh::option::setNumber("Mesh.MeshSizeFromCurvature", 0);
    gmsh::option::setNumber("Mesh.MeshSizeExtendFromBoundary", 0);
    // 开启后：Field设置的尺寸会自动扩展到边上
    gmsh::vectorpair faces;
    gmsh::model::getEntities(faces, 2);
    gmsh::model::setVisibility(faces, 0,true);

    std::set<int> meshedEdges;
    std::set<int> meshedFaces;

    for (auto& [dim, tag] : faces) {
        double size = 5.0 + tag * 10.0; // 每个face不同尺寸

        // 用 Field 控制面内部尺寸 
        // MathEval常数场
        int fieldTag = gmsh::model::mesh::field::add("MathEval");
        gmsh::model::mesh::field::setString(fieldTag, "F",
            std::to_string(size));
        gmsh::model::mesh::field::setAsBackgroundMesh(fieldTag);

        //  显示并网格化当前face 
        gmsh::model::setVisibility({ { 2, tag } }, 1,true);
        gmsh::model::mesh::generate(2);
        meshedFaces.insert(tag);

        //  清理Field，避免影响下一个face 
        gmsh::model::mesh::field::remove(fieldTag);

        // 记录已网格化的边
        gmsh::vectorpair edges;
        gmsh::model::getBoundary({ { 2, tag } }, edges, false, false, false);
        for (auto& [d, t] : edges) {
            if (d == 1)
                meshedEdges.insert(std::abs(t));
        }

        gmsh::write("final_mesh.obj");
        int i = 0;
    }

    gmsh::finalize();
    return 0;
}