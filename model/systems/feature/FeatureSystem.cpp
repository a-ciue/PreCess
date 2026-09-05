/**
 * @file FeatureSystem.cpp
 */
#include "FeatureSystem.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "ModelLayer.h"
#include "UndoStack.h"

#include <spdlog/spdlog.h>

namespace systems::feature {
const std::string FeatureSystem::name = "FeatureSystem";

FeatureSystem::FeatureSystem(ModelLayer& model_layer, core::EventBus& event_bus, UndoStack* undo_stack)
    : model_layer_(&model_layer)
    , event_bus_(&event_bus)
    , undo_stack_(undo_stack)
{
    on_feature_infos_changed_ = []() { };
}

FeatureSystem::~FeatureSystem()
{
    // 系统析构前先退出当前功能、再停用所有功能，让其清理状态（teardown 清理可能写模型，统一 flush）
    setFeatureActive("");
    for (auto&& [feature_name, entry] : entries_) {
        if (entry.handler) {
            entry.handler->teardown(*entry.context);
        }
    }
    model_layer_->flushNotifications();
}

bool FeatureSystem::registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler)
{
    if (!handler) {
        return false;
    }
    // 同名功能先注销旧的再替换
    if (entries_.count(meta_data.name)) {
        spdlog::warn("FeatureSystem::registerHandler: feature '{}' already registered, replacing it", meta_data.name);
        unregisterHandler(meta_data);
    }

    // 条目就地入库再装配：InteractionContext 持有本条目 InteractionState 的指针，
    // 先移动入库会使指针悬垂，故先 try_emplace 再装配上下文
    auto [it, inserted] = entries_.try_emplace(meta_data.name);
    FeatureEntry& entry = it->second;
    (void)inserted;

    // 装配功能上下文：参数集（空构造，setup 收集声明后移动赋值载入默认值，ctx 持引用不失效）
    // + 交互上下文 + 动态 provider（经系统转发，provider 后设置也生效）
    // 事件网关为每条目持有的视图：构造时绑定本功能的 undo 模式与显示名
    entry.undo_manual = meta_data.undo_manual;
    entry.event_gateway = std::make_unique<FeatureEventGateway>(
        *event_bus_, *model_layer_, undo_stack_, !meta_data.undo_manual, meta_data.display_name);
    entry.params = std::make_unique<FeatureParams>(std::vector<core::ArgType> { });

    // undo 上下文：Manual 功能经 staged 会话自控记录；无栈时 staged 接口空转容忍
    UndoContext undo_context;
    undo_context.automatic = !meta_data.undo_manual;
    if (UndoStack* stack = undo_stack_) {
        undo_context.beginStaged = [stack](std::string label, Index component_id) {
            return stack->beginStaged(std::move(label), component_id);
        };
        undo_context.commitStaged = [stack] { stack->commitStaged(); };
        undo_context.cancelStaged = [stack] { stack->cancelStaged(); };
        undo_context.revertStaged = [stack] { stack->revertStaged(); };
        undo_context.stagedActive = [stack] { return stack->stagedActive(); };
    } else {
        undo_context.beginStaged = [](std::string, Index) { return false; };
        undo_context.commitStaged = [] { };
        undo_context.cancelStaged = [] { };
        undo_context.revertStaged = [] { };
        undo_context.stagedActive = [] { return false; };
    }

    entry.context = std::make_unique<FeatureContext>(FeatureContext {
        *model_layer_,
        *entry.event_gateway,
        *entry.params,
        entry.interaction_context,
        [this]() { return active_model_provider_ ? active_model_provider_() : std::optional<Index> {}; },
        [this]() { return active_component_provider_ ? active_component_provider_() : std::optional<Index> {}; },
        [this](Index component_id) { return model_layer_->getComponentOperator(component_id); },
        std::move(undo_context),
    });

    // 功能信息：元数据部分先行（setup 声明的参数/菜单/按键在 setup 后补齐）
    auto info = std::make_unique<FeatureInfo>();
    info->name = meta_data.name;
    info->display_name = meta_data.display_name;
    info->description = meta_data.description;
    info->interactive = meta_data.interactive;
    entry.info = std::move(info);

    // 注入单激活约定：本功能 setActive(true) 时先下线其他功能的交互
    entry.interaction_context.deactivate_others_ = [this, feature_name = meta_data.name] {
        for (auto&& [other_name, other] : entries_) {
            if (other_name != feature_name) {
                other.interaction_state.active = false;
            }
        }
    };
    // 注入渲染刷新回调：功能经 requestRefresh() 通知 app 层拉取标注并重绘视口
    entry.interaction_context.render_refresh_ = [this] {
        if (render_refresh_callback_)
            render_refresh_callback_();
    };

    // setup 收集声明并订阅事件；失败则撤掉整个条目，不留下半注册状态
    FeatureRegistrar registrar;
    try {
        handler->setup(registrar, *entry.context);
    } catch (...) {
        entries_.erase(it);
        throw;
    }
    // setup 返回后载入参数默认值并补齐声明（*entry.params 移动赋值，ctx 持引用不失效）
    *entry.params = FeatureParams(registrar.argTypes());
    entry.info->arg_types = registrar.argTypes();
    entry.info->menus = registrar.menuItems();
    entry.info->key_bindings = registrar.keyBindings();
    entry.handler = std::move(handler);

    spdlog::info("FeatureSystem::registerHandler: Registered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
    return true;
}

void FeatureSystem::unregisterHandler(const HandlerMetaData& meta_data)
{
    auto it = entries_.find(meta_data.name);
    if (it == entries_.end()) {
        spdlog::warn("FeatureSystem::unregisterHandler: Feature '{}' not found", meta_data.name);
        return;
    }
    // 当前功能先按退出路径 deactivate（含交互下线），再 teardown（清理可能写模型，统一 flush）
    if (current_feature_ == meta_data.name) {
        setFeatureActive("");
    }
    if (it->second.handler) {
        flushAfterCallback([&] { it->second.handler->teardown(*it->second.context); });
    }
    entries_.erase(it);
    spdlog::info("FeatureSystem::unregisterHandler: Unregistered feature '{}'", meta_data.name);
    on_feature_infos_changed_();
}

std::any FeatureSystem::invoke(const std::string& unique_name)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end() || !it->second.handler) {
        spdlog::error("FeatureSystem::invoke: Feature '{}' not found", unique_name);
        return {};
    }

    // 操作边界（长期设施）：execute 返回后统一 flush 本次操作的组件变更通知；
    // 异常时先 flush 再重抛，保证部分写入的通知不丢。
    // undo 边界：Auto 功能包 beginOperation/commitOperation（label=功能显示名）；
    // Manual 功能只 flush（记录由插件经 ctx.undo 的 staged 会话自控）。
    // 异常路径提交而非丢弃 begin 过的操作：部分写入可撤销，与 flush 语义一致。
    FeatureEntry& entry = it->second;
    UndoStack* undo_stack = entry.undo_manual ? nullptr : undo_stack_;
    if (undo_stack)
        undo_stack->beginOperation(entry.info->display_name);
    try {
        std::any result = entry.handler->execute(*entry.context);
        if (undo_stack)
            undo_stack->commitOperation();
        model_layer_->flushNotifications();
        return result;
    } catch (...) {
        if (undo_stack)
            undo_stack->commitOperation();
        model_layer_->flushNotifications();
        throw;
    }
}

void FeatureSystem::flushAfterCallback(const std::function<void()>& fn)
{
    try {
        fn();
        model_layer_->flushNotifications();
    } catch (...) {
        model_layer_->flushNotifications();
        throw;
    }
}

bool FeatureSystem::setParameter(const std::string& unique_name, std::size_t index, core::ArgObject value)
{
    auto it = entries_.find(unique_name);
    if (it == entries_.end()) {
        spdlog::error("FeatureSystem::setParameter: Feature '{}' not found", unique_name);
        return false;
    }
    if (index >= it->second.params->count()) {
        spdlog::error("FeatureSystem::setParameter: param index {} out of range for feature '{}'", index, unique_name);
        return false;
    }
    it->second.params->setValue(index, value);
    event_bus_->publish(ParameterChangedEvent { unique_name, index, std::move(value) });
    return true;
}

std::vector<FeatureInfo*> FeatureSystem::getFeatureInfos()
{
    std::vector<FeatureInfo*> infos;
    infos.reserve(entries_.size());
    for (auto&& [feature_name, entry] : entries_) {
        infos.push_back(entry.info.get());
    }
    return infos;
}

const FeatureParams* FeatureSystem::params(const std::string& unique_name) const
{
    auto it = entries_.find(unique_name);
    return it == entries_.end() ? nullptr : it->second.params.get();
}

interaction::InteractionState* FeatureSystem::activeInteraction()
{
    for (auto&& [feature_name, entry] : entries_) {
        if (entry.info->interactive && entry.interaction_state.active) {
            return &entry.interaction_state;
        }
    }
    return nullptr;
}

bool FeatureSystem::setFeatureActive(const std::string& unique_name)
{
    // 幂等：与当前功能同名（含均为空）直接返回
    if (unique_name == current_feature_)
        return true;

    // 目标校验：功能未注册则不改变现状（当前功能保持进入态）
    auto target = entries_.end();
    if (!unique_name.empty()) {
        target = entries_.find(unique_name);
        if (target == entries_.end() || !target->second.handler) {
            spdlog::warn("FeatureSystem::setFeatureActive: feature '{}' not found", unique_name);
            return false;
        }
    }

    // 退出当前功能。定序：先功能回调再扳交互开关——deactivate() 中经 deferRefresh
    // 挂的渲染线程清理先于下线迁移的 notify 挂上，两种线程交错下都会被消费。
    // 回调可能写模型（如功能退出清理现场），经生命周期回调边界统一 flush
    if (!current_feature_.empty()) {
        auto cur = entries_.find(current_feature_);
        if (cur != entries_.end() && cur->second.handler) {
            flushAfterCallback([&] { cur->second.handler->deactivate(*cur->second.context); });
            if (cur->second.info->interactive)
                cur->second.interaction_context.setActive(false);
        }
        current_feature_.clear();
    }
    if (unique_name.empty())
        return true;

    // 进入新功能（同一定序：先功能回调备好现场，再上线交互）
    FeatureEntry& entry = target->second;
    flushAfterCallback([&] { entry.handler->activate(*entry.context); });
    if (entry.info->interactive)
        entry.interaction_context.setActive(true);
    current_feature_ = unique_name;
    return true;
}

void FeatureSystem::setOnFeatureInfosChanged(std::function<void()> callback)
{
    on_feature_infos_changed_ = std::move(callback);
}

void FeatureSystem::setActiveModelProvider(std::function<std::optional<Index>()> provider)
{
    active_model_provider_ = std::move(provider);
}

void FeatureSystem::setActiveComponentProvider(std::function<std::optional<Index>()> provider)
{
    active_component_provider_ = std::move(provider);
}

void FeatureSystem::setRenderRefreshCallback(std::function<void()> callback)
{
    render_refresh_callback_ = std::move(callback);
}

bool FeatureSystem::dispatchKeyEvent(const KeyEvent& event)
{
    // 原始事件流先广播，观察者总能收到；再做按键绑定路由并返回消费结果。
    // 按键绑定路由的 onKeyEvent 可能写模型（如 FeatureDemo 的缩放），
    // 路由返回后统一 flush（操作边界；异常时先 flush 再重抛）
    event_bus_->publish(event);
    try {
        const bool consumed = routeKeyEvent(event);
        model_layer_->flushNotifications();
        return consumed;
    } catch (...) {
        model_layer_->flushNotifications();
        throw;
    }
}

bool FeatureSystem::routeKeyEvent(const KeyEvent& event)
{
    if (!event.pressed) {
        return false; // 按键绑定仅在按下时触发；释放流由功能自行订阅 KeyEvent
    }
    bool consumed = false;
    for (auto&& [feature_name, entry] : entries_) {
        for (const KeyBinding& binding : entry.info->key_bindings) {
            if (binding.key == event.key && binding.modifiers == event.modifiers) {
                // undo 边界：按键回调路径按自动模式装配（快捷一次性调用即正确；
                // 重复调整类功能日后声明 Manual 迁移）。异常路径提交而非丢弃，与 invoke 一致。
                UndoStack* undo_stack = entry.undo_manual ? nullptr : undo_stack_;
                if (undo_stack)
                    undo_stack->beginOperation(entry.info->display_name);
                try {
                    consumed = entry.handler->onKeyEvent(event) || consumed;
                } catch (...) {
                    if (undo_stack)
                        undo_stack->commitOperation();
                    throw;
                }
                if (undo_stack)
                    undo_stack->commitOperation();
            }
        }
    }
    return consumed;
}
}
