/**
 * @file MeshBooleanPlugin.h
 * @brief 网格布尔 Feature 插件声明：将 MeshBooleanHandler 注册到功能系统
 *
 * 基于 CGAL Polygon Mesh Processing（corefinement）提供网格布尔运算：
 *   - 并集 / 交集 / 差集(A−B) / 差集(B−A)
 *
 * 对象 A 与对象 B 均由 Selector 参数显式指定（互不依赖对象树选中态）；
 * 内部使用 EPECK 精确内核，保证求交点的精确构造；结果写回对象 A，写路径
 * 经 ComponentOperator::replaceMesh 由系统层统一 flush 通知。
 */
#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "MeshBooleanHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {

/**
 * @brief 网格布尔 Feature 插件入口
 */
class MeshBooleanPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MeshBooleanPlugin/1.0" FILE "MeshBooleanPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MeshBooleanHandler, FeatureHandler>::get();
    }
};

} // namespace systems::feature
