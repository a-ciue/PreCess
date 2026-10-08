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

## 网格生成 Feature 导航声明

网格生成功能继承 `FeatureHandler`，在 `setup(FeatureRegistrar&, FeatureContext&)` 中一次声明参数和分类。JSON 只保留 `system: FeatureSystem` 与 Handler 身份，不解析导航 JSON 或旁置文件。

```cpp
void ExampleMesher::setup(FeatureRegistrar& registrar, FeatureContext&)
{
    registrar.addParameter({ ArgTypeEnum::Combo, "网格类型", "三角形,四边形" });
    registrar.addCategory({ "quadrilateral", "四边形网格生成", "", 20 });
    registrar.setLabel("四边形网格生成（示例方法）");
    registrar.setGroup("生成");
    registrar.setOrder(10);
    registrar.setCategoryDefault("quadrilateral", "网格类型", "1");
}
```

- `addCategory` 接收稳定 id、必填 title、可选 icon 和 order。相同 id 自动合并，一个功能可加入多个分类；`other` 为保留身份。参数使用 `addParameter` 声明，execute 从 `ctx.params` 读取。
- 未声明分类的 Feature 沿用普通菜单；已声明分类的功能进入网格生成三级导航，不重复出现在默认功能菜单。
- 三角形、四边形、四面体、六面体四个基础入口始终显示，无实现时显示“暂无可用算法”并禁用执行。扩展分类随最后一个提供者退出而消失。
- 同类描述应保持一致；冲突时选择功能唯一名按字典序最小的完整声明。分类按 order、id 排序。
- `setLabel` 为空时使用 display_name；setIcon 的资源由插件提供。分类默认值按参数名称匹配，Combo 使用选项索引字符串；进入分类时应用，用户随后可修改。
- 参数、激活、事件、后台任务和 undo 统一走 FeatureSystem；网格生成插件不调用 AlgorithmSystem 转发。

更新公共 Feature 声明后必须全量重建主程序、项目内插件、示例及独立 Addons，使用匹配 SDK、工具链和依赖，禁止混用旧 DLL。
