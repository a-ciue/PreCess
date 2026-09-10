/**
 * @file GeometryShapeWriter.h
 * @brief 几何创建类功能共用的 OCC 形状写入
 *
 * 服务于迁移到 FeatureSystem 的几何创建命令（创建点/长方体/圆柱体等）：
 * 按解析好的写入目标把 GeometryBuilder 产出的 OCC Shape 组织进模型层。
 * 写入目标的用户提示语属于界面层，由各功能就近处理；本类只负责写入。
 * undo 记录与变更通知由调用方的操作边界（如 FeatureSystem::invoke）统一负责，
 * 本类不建边界、不发通知。
 *
 * @date 2026/9/10
 */
#pragma once

#include "Core.h"

#include <TopoDS_Shape.hxx>

#include <string>

class ModelLayer;

/**
 * @brief 按写入目标把 OCC 形状组织进模型层
 */
class GeometryShapeWriter {
public:
    /**
     * @brief 解析后的写入目标（-1 表示"新建"）
     */
    struct WriteTarget {
        Index model_id { -1 };
        Index component_id { -1 };
    };

    /**
     * @brief 按解析目标把 OCC Shape 写入模型层
     *
     * 组件目标优先：追加到既有组件几何；否则在目标模型下新建几何组件；
     * 无目标模型时新建临时模型（命名 "temp_<component_name>"）承载。
     * 写入即标脏，通知由调用方的操作边界 flush。
     * @return 新建或更新的组件 ID
     * @throw std::invalid_argument 目标模型/组件不存在
     */
    static Index writeShape(
        ModelLayer& model_layer,
        const WriteTarget& target,
        std::string component_name,
        TopoDS_Shape shape);
};
