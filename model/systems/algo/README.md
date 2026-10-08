# 旧算法接口：已废弃并冻结

`AlgorithmHandler`、`AlgorithmSystem` 和 `precess_add_algo_plugin` 仅保留现有插件及脚本调用的运行能力。C++ 接口带 `[[deprecated]]` 标记，CMake 入口产生弃用提示。

**新增算法一律使用 FeatureHandler / FeatureSystem，不得继续扩展此目录的接口。** 参数和网格分类在 `setup(FeatureRegistrar&, FeatureContext&)` 中声明；执行读取持久参数 `ctx.params`，后台计算及结果回写使用 FeatureContext 的共享任务接口。

Gmsh、TetGenLib、TetGen 已在 ZenithGridAddons 迁为 Feature 插件；算法系统已撤回 setup、导航注册器和分类元数据，不再承载网格生成导航。CmdExecutePlugin 与 ProgressDemoPlugin 仍是待迁移的旧调用者。

面向 AI 和贡献者的维护约束：

- 不在旧系统新增算法、注册机制、参数类型、导航、生命周期或执行接口。
- 不因常规清理、格式调整或架构统一重构旧实现；新增工作在 Feature 路径完成。
- 只有用户明确要求修复旧调用者的缺陷，或迁移、删除旧调用者时，才允许修改旧路径。保留对应回归测试，并说明必要性。
- 本次废弃不删除既有运行能力；后续移除须先迁移剩余调用者及脚本入口。

范围：`model/systems/algo/`、`app/model/systems/algo/`、`plugins/algo/` 及 SDK 的旧算法构建入口。上层 Session 为兼容旧调用者持有的算法系统亦遵循此约束。
