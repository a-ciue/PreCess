/**
 * @file MeshRepairPlugin.h
 * @brief 网格修复 Feature 插件声明：将 MeshRepairHandler 注册到功能系统
 *
 * 基于 CGAL Polygon Mesh Processing（PMP）提供三种修复操作：
 *   - 孔洞三角化填补（逐 border_cycle 跟踪成功/失败计数）
 *   - 自相交面检测（不修改网格，仅返回报告）
 *   - 退化面清理
 *
 * 通过 Component 选择器参数显式指定目标 Component，避免依赖对象树选中态；
 * 写路径经 ComponentOperator::replaceMesh 由系统层统一 flush 通知。
 */
#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "MeshRepairHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {

/**
 * @brief 网格修复 Feature 插件入口
 */
class MeshRepairPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MeshRepairPlugin/1.0" FILE "MeshRepairPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MeshRepairHandler, FeatureHandler>::get();
    }
};

}