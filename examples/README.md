# ZenithGrid / PreCess 插件示例

源码与测试随 SDK 的 Development 组件分发，默认不构建示例 DLL。示例自有代码与测试统一采用 LGPLv3，第三方组件遵循各自许可证。

| 示例 | 用途 |
| --- | --- |
| ExternalPlugin | 最小功能插件骨架 |
| FeatureDemoPlugin | 参数、按键、模型事件与同步写入 |
| ProgressDemoPlugin | 旧算法接口的进度与取消兼容示例；新增算法使用 Feature 与 TaskDemoPlugin 范式 |
| TaskDemoPlugin | 纯计算、影子组件、类型化回写任务 |
| ScalePreviewPlugin | 预览、后台缩放、确认与取消 |

## SDK 独立开发

仅使用已安装 SDK 和配套依赖。复制整个 examples 到可写目录，保留 ExampleProject.cmake；使用 CMake 3.27+、Ninja、C++20 工具链，Windows 先加载 x64 开发环境。

```powershell
# 在 examples 所在目录执行；单个插件可改为 -S examples/TaskDemoPlugin。
cmake -S examples -B build/examples -G Ninja "-DCMAKE_PREFIX_PATH=<SDK 前缀或目标主程序安装目录>;<PreCess-deps 依赖根>" "-DPreCess_DIR=<SDK 前缀>/lib/cmake/PreCess" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/examples
ctest --test-dir build/examples --output-on-failure
cmake --install build/examples --prefix "<目标主程序安装目录>" --component AllPlugins
```

- 不固定 SDK 或编译器版本；按 SDK 的 ABI 提示核对工具链、依赖与目标主程序，Debug/非 Debug 不混用。
- DLL 位于 build/examples/plugins；示例默认安装到目标前缀的 plugins/。macOS 或其他扫描目录用 PRECESS_EXAMPLE_PLUGIN_INSTALL_DIR 指定，绝对目录不受 --prefix 改写。
- 开发、测试和加载细节见 SDK 的 skills/zenithgrid-external-plugin-development/SKILL.md；没有启动主程序验证时，只报告构建与测试结果。

## 示例随主程序构建

项目内插件（plugins/）直接继承主工程；本目录示例则保留源内、源外两套入口。同一份 CMakeLists 中，project、SDK 查找和显式安装只在 if(NOT PRECESS_PLUGIN_IN_TREE) 下执行；随主程序构建时跳过这些步骤，继承主工程的目标、依赖和自动安装规则。

在软件仓库根目录执行：

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=<PreCess-deps 依赖根>" -DCMAKE_BUILD_TYPE=Debug -DPRECESS_BUILD_EXAMPLES=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix "<安装前缀>" --component AllPlugins
```

- PRECESS_BUILD_EXAMPLES 默认 OFF；开启后示例加入 AllPlugins，DLL 与正式插件共用 ${CMAKE_BINARY_DIR}/plugins，安装到同一个 plugins/。macOS 两者共用应用 bundle 的 Contents/plugins。
- 修改共享头文件后构建全部插件；仅构建 PreCess 主程序目标不会重建动态插件。运行验证使用本次构建的主程序。
- PRECESS_PLUGIN_IN_TREE 由构建入口管理，不手动设置。示例保留独立入口；项目内插件不需要这套 SDK 入口。以上命令的产物均放在独立 build 目录。

模型写入、任务和线程契约以接口头文件为准。示例使用活动组件是教学简化，正式功能优先通过 Selector 选择目标。

## 通用 Feature 分类导航

新增算法、编辑和通用业务功能使用 `FeatureHandler`。在 setup 中声明参数，经 `registrar.navigation()` 声明父入口与子功能的关联；分类不限于网格生成，JSON 和旁置文件不提供导航。

```cpp
void ExampleSolver::setup(FeatureRegistrar& registrar, FeatureContext&)
{
    registrar.addParameter({ ArgTypeEnum::Int, "迭代数", "100" });
    auto& navigation = registrar.navigation();
    navigation.addCategory({ "analysis", "分析", "", 20, "功能/求解" });
    navigation.setLabel("稳态求解器");
}
```

`FeatureCategory` 第五个字段 `menu_path` 指定“菜单/分组”；留空时继承宿主同 id 的路径，解析后仍为空则依次回退到首个子功能的菜单贡献、“功能”菜单。父入口可以关联 0、1 或多个功能：0 显示空态并禁用执行，1 直接显示参数，多个显示子功能选择器。需要常驻的入口由产品宿主注入，FeatureSystem 不固定网格分类。

完整字段、校验、聚合、宿主配置和网格生成实例见源码树的 [SDK 导航 reference](../sdk/skills/zenithgrid-external-plugin-development/references/development.md#通用-feature-分类导航)；已安装 SDK 内对应 `skills/zenithgrid-external-plugin-development/references/development.md`。参数、激活、事件、任务与 undo 均走 FeatureSystem，不转发到已废弃的算法/编辑系统；本次不迁移或删除已有旧插件。

空导航继续使用普通菜单，既有普通 Feature 插件无需修改导航源码。公共 Feature 头文件变化后，必须用匹配 SDK、工具链和依赖全量重建主程序、项目内插件、示例及独立 Addons，禁止混用旧 DLL。
