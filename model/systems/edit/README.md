# 旧编辑接口：已废弃并冻结

`EditHandler`、`EditSystem` 和 `precess_add_edit_plugin` 仅保留现有插件及脚本调用的运行能力。C++ 接口带 `[[deprecated]]` 标记，CMake 入口产生弃用提示。

新增编辑功能使用 `FeatureHandler` / `FeatureSystem`，在 `setup(FeatureRegistrar&, FeatureContext&)` 声明参数、菜单或分类导航；执行读取 `ctx.params`，模型写入及后台回写遵循 `FeatureContext` 的受控写和任务契约。

现有 CreateFacePlugin、DeleteFacePlugin 继续使用旧编辑入口。本次仅标记废弃并冻结，不迁移或删除已有插件，也不改变其执行和 undo 行为。

维护规则：

- 不新增旧编辑插件，不扩展旧系统的注册、参数、导航或执行接口。
- 不为常规清理、格式调整或架构统一重构旧实现。
- 仅为既有调用者缺陷修复，或后续明确安排的迁移、删除修改旧路径；保留对应回归测试并说明必要性。
- 移除旧系统前先迁移剩余插件、Qt 适配层与脚本入口。

范围：`model/systems/edit/`、`app/model/systems/edit/`、`plugins/edit/` 及 SDK 的旧编辑构建入口；Session 为兼容旧调用者持有的编辑系统同样适用。
