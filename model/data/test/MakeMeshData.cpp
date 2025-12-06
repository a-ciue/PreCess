#include "MakeMeshData.h"
#include <iostream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
MeshData MakeMeshData()
{
    MeshData data;
    data.init();
    // 更复杂的测试模型（删减版本）：
    // Solid: 1个六面体 (Hexahedron) + 1个楔体 (Wedge)
    // Faces: 立方体保留1个四边形面，楔体保留2个三角形面，另外保留2个独立面（1四边形+1三角形）

    // 顶点说明：
    // 0-7   : 立方体
    // 8-11  : 独立四边形
    // 12-13 : 第一条独立线段（用于edges）
    // 14-19 : 楔体 (wedge)
    // 20-22 : 独立三角形
    data.vertex_positions_ = {
        // 立方体 (0-7)
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0, 1.0 },
        // 独立四边形 (8-11)
        { 1.5, 0.0, 0.0 }, { 2.5, 0.0, 0.0 }, { 2.5, 1.0, 0.0 }, { 1.5, 1.0, 0.0 },
        // 独立线段1 (12-13)
        { 0.0, 1.5, 0.0 }, { 1.0, 1.5, 0.0 },
        // 楔体 Wedge (14-19)
        { 3.0, 0.0, 0.0 }, { 4.0, 0.0, 0.0 }, { 3.5, 1.0, 0.0 },
        { 3.0, 0.0, 1.0 }, { 4.0, 0.0, 1.0 }, { 3.5, 1.0, 1.0 },
        // 独立三角形 (20-22)
        { 2.0, 1.5, 0.0 }, { 2.5, 1.5, 0.0 }, { 2.25, 2.0, 0.0 }
    };

    // 面（删减）：1个立方体四边形 + 2个楔体三角形 + 2个独立面
    data.face_vertices_ = {
        // 立方体 1 个四边形
        0, 3, 2, 1, // 面0
        // 楔体 2 个三角形
        14, 15, 16, // 面1
        17, 19, 18, // 面2
        // 独立四边形
        8, 9, 10, 11, // 面3
        // 独立三角形
        20, 21, 22 // 面4
    };
    data.face_vertices_offset_ = {
        0, // 面0 起始
        4, // 面1 起始
        7, // 面2 起始
        10, // 面3 起始
        14, // 面4 起始
        17 // 结束
    };

    // 边：保持与原示例一致，覆盖立方体、楔体和独立线段
    data.edge_vertices_ = {
        // 立方体 12 条边 (0-7)
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
        // 独立线段1
        12, 13,
        // 楔体 9 条边 (底3 + 顶3 + 竖向3)
        14, 15, 15, 16, 16, 14,
        17, 18, 18, 19, 19, 17,
        14, 17, 15, 18, 16, 19,
        // 独立线段2 (三角的一条边)
        20, 21
    };

    // 体：1个六面体 + 1个楔体
    // data.solid_types_ = { VTK_HEXAHEDRON, VTK_WEDGE };
    data.solid_types_ = { 12, 13 };
    data.solid_vertices_ = {
        // Hex (8点)
        0, 1, 2, 3, 4, 5, 6, 7,
        // Wedge (6点)
        14, 15, 16, 17, 18, 19
    };
    data.solid_vertices_offset_ = { 0, 8, 14 }; // 2 个 cell

    // polyhedral 相关留空
    data.solid_faces_offset_ = { 0, 0, 0 };

    // Block: 根据删减后的面索引重新划分
    // Block1 -> 立方体 1 个面 (0)
    // Block2 -> 楔体    2 个面 (1,2)
    // Block3 -> 独立面  2 个面 (3,4)
    data.blocks_.clear();
    {
        auto b1 = std::make_unique<Block>();
        b1->patchIDs = { 1 }; // Patch 1 包含面0
        data.blocks_[1] = std::move(b1);

        auto b2 = std::make_unique<Block>();
        b2->patchIDs = { 2 }; // Patch 2 包含面1,2
        data.blocks_[2] = std::move(b2);

        auto b3 = std::make_unique<Block>();
        b3->patchIDs = { 3, 4 }; // Patch 3,4 分别包含面3,4
        data.blocks_[3] = std::move(b3);
    }

    return data;
}

MeshData MakeMeshDataWithAtri()
{
    MeshData data;
    data.init();

    // 顶点位置 (0-22)
    data.vertex_positions_ = {
        // 立方体 (0-7)
        { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0, 1.0 },
        // 独立四边形 (8-11)
        { 1.5, 0.0, 0.0 }, { 2.5, 0.0, 0.0 }, { 2.5, 1.0, 0.0 }, { 1.5, 1.0, 0.0 },
        // 独立线段 (12-13)
        { 0.0, 1.5, 0.0 }, { 1.0, 1.5, 0.0 },
        // 楔体 (14-19)
        { 3.0, 0.0, 0.0 }, { 4.0, 0.0, 0.0 }, { 3.5, 1.0, 0.0 },
        { 3.0, 0.0, 1.0 }, { 4.0, 0.0, 1.0 }, { 3.5, 1.0, 1.0 },
        // 独立三角形 (20-22)
        { 2.0, 1.5, 0.0 }, { 2.5, 1.5, 0.0 }, { 2.25, 2.0, 0.0 }
    };

    // 面定义
    data.face_vertices_ = {
        // 立方体 1 个四边形 (面0)
        0, 3, 2, 1,
        // 楔体 2 个三角形 (面1,2)
        14, 15, 16,
        17, 19, 18,
        // 独立四边形 (面3)
        8, 9, 10, 11,
        // 独立三角形 (面4)
        20, 21, 22
    };
    data.face_vertices_offset_ = { 0, 4, 7, 10, 14, 17 }; // 5个面

    // 边定义
    data.edge_vertices_ = {
        // 立方体 12 条边
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
        // 独立线段
        12, 13,
        // 楔体 9 条边
        14, 15, 15, 16, 16, 14,
        17, 18, 18, 19, 19, 17,
        14, 17, 15, 18, 16, 19,
        // 独立三角形边
        20, 21
    };

    const size_t num_points = data.vertex_positions_.size(); // 23 points
    const size_t num_faces = data.face_vertices_offset_.size() - 1; // 5 faces

    //  添加顶点一元属性 (VertexScalar)
    std::vector<double> vertex_scalar;
    for (size_t i = 0; i < num_points; i++) {
       
        vertex_scalar.push_back(i);
    }
    data.vertex_attributes_["VertexScalar"] = vertex_scalar; // 顶点标量属性
    // 输出点一元属性
    for each (double var in vertex_scalar) {
        std::cout << "vertex_scalar" <<var << std::endl;
    }
    //  添加面一元属性 (FaceScalar)
    std::vector<double> face_scalar;
    for (size_t i = 0; i < num_faces; ++i) {
        size_t start = data.face_vertices_offset_[i];
        size_t end = data.face_vertices_offset_[i + 1];
        double sum_x = 0, sum_y = 0, sum_z = 0;

        for (size_t j = start; j < end; ++j) {
            size_t idx = data.face_vertices_[j];
            sum_x += data.vertex_positions_[idx][0];
            sum_y += data.vertex_positions_[idx][1];
            sum_z += data.vertex_positions_[idx][2];
        }
        face_scalar.push_back((sum_x + sum_y * (sum_z + i)) / (end - start) + i);
    }
    data.face_attributes_["FaceScalar"] = face_scalar; // 面标量属性
    //输出面一元属性
    for each (double var in face_scalar ) {
        std::cout << "faceScalar" << std::endl;
    }
    // 顶点颜色 (三元组)
    std::vector<double> vertex_color_3;
    for (int i = 0; i < num_points; ++i) {
            switch (i % 6) { // 6种颜色
            case 0:
                vertex_color_3.insert(vertex_color_3.end(), { 1.0, 0.0, 0.0 });
                break; // 纯红
            case 1:
                vertex_color_3.insert(vertex_color_3.end(), { 1.0, 0.5, 0.0 });
                break; // 橙
            case 2:
                vertex_color_3.insert(vertex_color_3.end(), { 1.0, 1.0, 0.0 });
                break; // 黄
            case 3:
                vertex_color_3.insert(vertex_color_3.end(), { 0.0, 1.0, 0.0 });
                break; // 绿
            case 4:
                vertex_color_3.insert(vertex_color_3.end(), { 0.0, 0.0, 1.0 });
                break; // 蓝
            case 5:
                vertex_color_3.insert(vertex_color_3.end(), { 0.5, 0.0, 1.0 });
                break; // 紫
            }
    }
        data.vertex_attributes_["vertex_color_3"] = vertex_color_3;

    // 面颜色 (三元组)
    //std::cout << "num_faces" << num_faces << std::endl;
    std::vector<double> face_color_3;
    for (int i = 0; i < num_faces; ++i) {
        switch (i % 5) {
        case 0:
            face_color_3.insert(face_color_3.end(), { 0.0, 1.0, 0.0 });
            break; // 红
        case 1:
            face_color_3.insert(face_color_3.end(), { 1.0, 0.0, 0.0 });
            break; // 绿
        case 2:
            face_color_3.insert(face_color_3.end(), { 1.0, 1.0, 0.0 });
            break; // 蓝
        case 3:
            face_color_3.insert(face_color_3.end(), { 0.5, 0.0, 1.0 });
            break; // 黄
        case 4:
            face_color_3.insert(face_color_3.end(), { 0.7, 0.0, 0.7 });
            break; // 紫
        }
    }
    data.face_attributes_["face_color_3"] = face_color_3;
    // 顶点向量属性1 press (三元组)
    std::array<std::array<double, 3>, 8> p_vector = { { { { 0, 0, 1 } },
        { { 1, 0, 0 } },
        { { 1, 1, 0 } },
        { { 0, 1, 0 } },
        { { 0, 0, 1 } },
        { { 1, 0, 1 } },
        { { 1, 1, 1 } },
        { { 0, 1, 1 } } } };
    std::vector<double> vertex_vector;
    for (size_t i = 0; i < num_points; ++i) {
        const auto& vec = p_vector[i % p_vector.size()];
        vertex_vector.push_back(vec[0]);
        vertex_vector.push_back(vec[1]);
        vertex_vector.push_back(vec[2]);
    }
    data.vertex_attributes_["vertex_press_3"] = vertex_vector;
    // 顶点向量属性2 normal (三元组)
    std::array<std::array<double, 3>, 8> n_vector = { { { { 0, 0, 1 } },
        { { -3, -2, 0 } },
        { { -3, 3, 0 } },
        { { 0, 3, -2 } },
        { { 0, -1, 1 } },
        { { 1, -4, 1 } },
        { { 1,2, 1 } },
        { { -2, 1, 5 } } } };
    std::vector<double> vertex_vector2;
    for (size_t i = 0; i < num_points; ++i) {
        const auto& vec = n_vector[i % n_vector.size()];
        vertex_vector2.push_back(vec[0]);
        vertex_vector2.push_back(vec[1]);
        vertex_vector2.push_back(vec[2]);
    }
    data.vertex_attributes_["vertex_normal_3"] = vertex_vector2;

    // 面向量属性 (三元组)
    std::array<std::array<double, 3>, 5> f_vector = { { { { 0, 0, 1 } },
        { { 1, 0, 0 } },
        { { 0, 1, 0 } },
        { { -1, 0, 0 } },
        { { 0, -1, 0 } } } };
    std::vector<double> face_vector;
    for (size_t i = 0; i < num_faces; ++i) {
        const auto& vec = f_vector[i % f_vector.size()];
        face_vector.push_back(vec[0]);
        face_vector.push_back(vec[1]);
        face_vector.push_back(vec[2]);
    }
    data.face_attributes_["face_normal_3"] = face_vector;
    return data;
}

MeshData MakeMeshDataWithUV()
{
    MeshData data;
    data.init();

    // ===== 创建贴合球面的半球 (19个点，30个三角形面) =====
    // 半球中心在原点(0,0,0)，半径1.0，z>=0
    std::vector<std::array<double, 3>> positions;

    // 1. 顶部点 (0°纬度)
    positions.push_back({ -1.0, -1.0, 1.0 }); // 索引0

    // 2. 30°纬度 (6个点)
    const double theta1 = 30.0 * M_PI / 180.0;

    for (int i = 0; i < 6; i++) {
        double phi = i * 60.0 * M_PI / 180.0;
        double x = sin(theta1) * cos(phi)-1;
        double y = sin(theta1) * sin(phi)-1;
        double z = cos(theta1);
        positions.push_back({ x, y, z });
    }

    // 3. 60°纬度 (6个点)
    const double theta2 = 60.0 * M_PI / 180.0;
    for (int i = 0; i < 6; i++) {
        double phi = i * 60.0 * M_PI / 180.0;
        double x = sin(theta2) * cos(phi)-1;
        double y = sin(theta2) * sin(phi)-1;
        double z = cos(theta2);
        positions.push_back({ x, y, z });
    }

    // 4. 90°纬度 (底部圆环，6个点)
    const double theta3 = 90.0 * M_PI / 180.0;
    for (int i = 0; i < 6; i++) {
        double phi = i * 60.0 * M_PI / 180.0;
        double x = sin(theta3) * cos(phi)-1;
        double y = sin(theta3) * sin(phi)-1;
        double z = cos(theta3);
        positions.push_back({ x, y, z });
    }

    data.vertex_positions_ = positions;

    // ===== 构建30个三角形面 =====
    std::vector<int> faces;

    // 1. 顶部(0)与30°纬度(1-6)的6个三角形
    for (int i = 0; i < 6; i++) {
        faces.push_back(0);
        faces.push_back(1 + i);
        faces.push_back(1 + (i + 1) % 6);
    }

    // 2. 30°纬度(1-6)与60°纬度(7-12)的12个三角形
    for (int i = 0; i < 6; i++) {
        // 三角形1: 30°点i, 30°点i+1, 60°点i
        faces.push_back(1 + i);
        faces.push_back(1 + (i + 1) % 6);
        faces.push_back(7 + i);

        // 三角形2: 30°点i+1, 60°点i+1, 60°点i
        faces.push_back(1 + (i + 1) % 6);
        faces.push_back(7 + (i + 1) % 6);
        faces.push_back(7 + i);
    }

    // 3. 60°纬度(7-12)与90°纬度(13-18)的12个三角形
    for (int i = 0; i < 6; i++) {
        // 三角形1: 60°点i, 60°点i+1, 90°点i
        faces.push_back(7 + i);
        faces.push_back(7 + (i + 1) % 6);
        faces.push_back(13 + i);

        // 三角形2: 60°点i+1, 90°点i+1, 90°点i
        faces.push_back(7 + (i + 1) % 6);
        faces.push_back(13 + (i + 1) % 6);
        faces.push_back(13 + i);
    }

    data.face_vertices_ = faces;

    // 面偏移量 (30个面，每个面3个点)
    std::vector<int> face_offsets;
    for (int i = 0; i <= 30; i++) {
        face_offsets.push_back(i * 3);
    }
    data.face_vertices_offset_ = face_offsets;
    // ===== 半球面构建完成 =====

    // ===== 添加球面UV纹理坐标 =====
    std::vector<double> uv;
    for (size_t i = 0; i < data.vertex_positions_.size(); i++) {
        double theta = 0.0, phi = 0.0;

        // 顶部点 (索引0)
        if (i == 0) {
            theta = 0.0;
            phi = 0.0;
        }
        // 30°纬度 (索引1-6)
        else if (i <= 6) {
            theta = 30.0 * M_PI / 180.0;
            phi = (i - 1) * 60.0 * M_PI / 180.0;
        }
        // 60°纬度 (索引7-12)
        else if (i <= 12) {
            theta = 60.0 * M_PI / 180.0;
            phi = (i - 7) * 60.0 * M_PI / 180.0;
        }
        // 90°纬度 (索引13-18)
        else {
            theta = 90.0 * M_PI / 180.0;
            phi = (i - 13) * 60.0 * M_PI / 180.0;
        }

        // UV映射: U=phi/(2π), V=(π/2 - theta)/(π/2)
        double u = phi / (2 * M_PI);
        double v = (M_PI / 2 - theta) / (M_PI / 2);

        uv.push_back(u);
        uv.push_back(v);
    }
    data.vertex_attributes_["vertex_uv_2"] = uv;
    // ===== UV添加完成 =====

    return data;
}