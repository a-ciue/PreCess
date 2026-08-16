#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ScalePreviewHandler.h"
#include "SystemPluginManager.h"
#include "UndoStack.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <string>

using namespace systems;
using namespace systems::feature;

#ifndef SCALE_PREVIEW_PLUGIN_PATH
#define SCALE_PREVIEW_PLUGIN_PATH ""
#endif

namespace {
constexpr const char* kFeatureName = "ScalePreview";
// 参数下标：0 缩放因子（Float），1 预览（Button），2 取消（Button）
constexpr std::size_t kParamScale = 0;
constexpr std::size_t kParamPreview = 1;
constexpr std::size_t kParamCancel = 2;
const std::array<double, 3> kOriginal { 1.0, 2.0, 3.0 };

struct CountingObserver : ModelObserver {
    int component_changed_count { 0 };

    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyComponentChanged(Index) override { ++component_changed_count; }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
};

//! @brief 构造单点组件模型并入池，返回 component_id
Index addSingleComponentModel(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { kOriginal };

    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    Index model_id = model_layer.addModel("test_model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

//! @brief 功能元数据：与 ScalePreviewPlugin.json 一致（undo_manual = true）
HandlerMetaData scalePreviewMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = kFeatureName;
    meta_data.display_name = "缩放预览演示";
    meta_data.undo_manual = true;
    return meta_data;
}

std::array<double, 3> firstVertex(ModelLayer& model_layer, Index component_id)
{
    return model_layer.findComponent(component_id)->mesh->vertex_positions_[0];
}

//! @brief 公共夹具：ModelLayer + UndoStack + FeatureSystem 挂接（参考 TestFeatureUndo.cpp）
struct ScalePreviewFixture {
    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    FeatureSystem system { mgr, bus, &stack };

    ScalePreviewFixture() { mgr.setUndoRecorder(&stack); }

    //! @brief 建组件、清栈（入池的结构记录不计入断言）、注册功能并注入活动组件 provider
    Index setupFeature()
    {
        const Index component_id = addSingleComponentModel(mgr);
        stack.clear();
        FeatureSystem::SystemHandlerPtr handler { new ScalePreviewHandler };
        REQUIRE(system.registerHandler(scalePreviewMetaData(), std::move(handler)));
        system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });
        return component_id;
    }
};
}

TEST_CASE("ScalePreviewPlugin dll registers into FeatureSystem via SystemPluginManager", "[ScalePreviewPlugin]")
{
    int argc = 1;
    char arg0[] = "TestScalePreviewPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    CountingObserver obs;
    ModelLayer model_layer { &obs };
    core::EventBus bus;
    UndoStack stack { model_layer };
    model_layer.setUndoRecorder(&stack);
    FeatureSystem feature_system(model_layer, bus, &stack);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据（含 "undo": "manual"）+ 系统注册器
    REQUIRE(plugin_manager.registerPlugin(SCALE_PREVIEW_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "ScalePreview");
    REQUIRE(infos[0]->display_name == "缩放预览演示");
    REQUIRE(infos[0]->arg_types.size() == 3);
    REQUIRE(infos[0]->menus.size() == 1);
    REQUIRE(infos[0]->menus[0].menu_path == "示例");

    // json 的 "undo": "manual" 生效的行为证据：预览按钮开启 staged 会话且边界不自动提交
    const Index component_id = addSingleComponentModel(model_layer);
    stack.clear();
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });
    REQUIRE(feature_system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(stack.stagedActive());
    // 栈内无记录：undo 指向 staged 会话本身（此时 undo = 回滚预览）
    REQUIRE(stack.undoLabel() == "缩放预览");

    // 预览写经取消按钮回滚，全程不成记录
    REQUIRE(feature_system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(firstVertex(model_layer, component_id) == std::array<double, 3> { 2.0, 4.0, 6.0 });
    REQUIRE(feature_system.setParameter(kFeatureName, kParamCancel, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE_FALSE(stack.stagedActive());
    REQUIRE(firstVertex(model_layer, component_id) == kOriginal);
    REQUIRE_FALSE(stack.canUndo());

    plugin_manager.unregisterPlugin(SCALE_PREVIEW_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("ScalePreview preview button opens session without record but still flushes", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();
    const int notify_before = f.obs.component_changed_count;

    // 默认因子 1.0：预览不改坐标，但 editableMesh 获取即标脏
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));

    // Manual 网关只 flush 不成记录；staged 会话保持打开（通知照发）
    REQUIRE(f.stack.stagedActive());
    REQUIRE(f.obs.component_changed_count == notify_before + 1);
    // canUndo 在 staged 打开时表示"可取消预览"（undo=cancelStaged），记录有无须在会话
    // 关闭后断言：取消会话后栈仍为空，证明预览写未入栈
    f.stack.cancelStaged();
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview full cycle: preview, retry with new factor, confirm via menu, undo", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    // staged 未打开时改因子只更新参数，不改模型
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);

    // 预览按钮：开 staged 会话并按 2.0 预览
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(f.stack.stagedActive());
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });

    // 预览重试：revertStaged 回 before₀ 再按新因子 3.0 重写（绝对因子，非累计 ×6）
    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(3.0)));
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 3.0, 6.0, 9.0 });
    REQUIRE(f.stack.stagedActive());
    // 预览期不成记录由收尾的"恰一条记录"断言覆盖（staged 打开时 canUndo 恒 true，
    // 语义为"可取消预览"，不能用于判记录有无）

    // 菜单触发 execute = 确认：before₀ + 当前状态恰记一条
    f.system.invoke(kFeatureName);
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(f.stack.canUndo());
    REQUIRE(f.stack.undoLabel() == "缩放预览");

    // undo 回原值；栈清空证明恰一条记录
    f.stack.undo();
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview cancel rolls back preview without record", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(firstVertex(f.mgr, cid) == std::array<double, 3> { 2.0, 4.0, 6.0 });

    // 取消：回滚到 before₀ 并关闭会话，不成记录
    REQUIRE(f.system.setParameter(kFeatureName, kParamCancel, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview deactivate cancels an open staged session", "[ScalePreviewPlugin]")
{
    ScalePreviewFixture f;
    const Index cid = f.setupFeature();

    REQUIRE(f.system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE(f.system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
    REQUIRE(f.stack.stagedActive());

    // 功能注销触发 deactivate：staged 未关须 cancelStaged 兜底（插件职责，AGENTS.md 约定）
    f.system.unregisterHandler(scalePreviewMetaData());
    REQUIRE_FALSE(f.stack.stagedActive());
    REQUIRE(firstVertex(f.mgr, cid) == kOriginal);
    REQUIRE_FALSE(f.stack.canUndo());
}

TEST_CASE("ScalePreview teardown with open staged session is safe", "[ScalePreviewPlugin]")
{
    // 对应程序退出路径：staged 未关时宿主析构——~FeatureSystem 停用 handler，
    // deactivate 经 ctx.undo.cancelStaged() 回滚预览。本用例用作用域模拟宿主的
    // 显式有序拆解（见 QModelManager 析构注释）：FeatureSystem 先析构，
    // UndoStack / ModelLayer 存活，拆解链不崩且预览被回滚
    CountingObserver obs;
    ModelLayer mgr { &obs };
    core::EventBus bus;
    UndoStack stack { mgr };
    mgr.setUndoRecorder(&stack);
    const Index cid = addSingleComponentModel(mgr);
    stack.clear();

    {
        FeatureSystem system { mgr, bus, &stack };
        FeatureSystem::SystemHandlerPtr handler { new ScalePreviewHandler };
        REQUIRE(system.registerHandler(scalePreviewMetaData(), std::move(handler)));
        system.setActiveComponentProvider([cid]() { return std::optional<Index> { cid }; });

        REQUIRE(system.setParameter(kFeatureName, kParamScale, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
        REQUIRE(system.setParameter(kFeatureName, kParamPreview, core::ArgObject::create<ArgTypeEnum::Button>(1)));
        REQUIRE(stack.stagedActive());
        // 作用域结束：~FeatureSystem → deactivate → cancelStaged（栈与模型层仍存活）
    }

    REQUIRE_FALSE(stack.stagedActive());
    REQUIRE(firstVertex(mgr, cid) == kOriginal);
    REQUIRE_FALSE(stack.canUndo());
}
