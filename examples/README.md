# ZenithGrid / PreCess 插件示例

源码与测试随 SDK 的 Development 组件分发，默认不构建示例 DLL。示例自有代码与测试统一采用 LGPLv3，第三方组件遵循各自许可证。

| 示例 | 用途 |
| --- | --- |
| ExternalPlugin | 最小功能插件骨架 |
| FeatureDemoPlugin | 参数、按键、模型事件与同步写入 |
| ProgressDemoPlugin | 算法进度与取消 |
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

## 算法插件的导航声明

算法导航统一通过 `AlgorithmHandler::setup(AlgorithmRegistrar&)` 声明。JSON 只保留 `system`、`handler.name`、`handler.display_name` 等身份信息；不再解析 `handler.navigation` 或 DLL 旁置导航文件。

```cpp
#include "AlgorithmHandler.h"
#include "AlgorithmRegistrar.h"

class ExampleQuadMesher : public systems::algo::AlgorithmHandler {
public:
    void setup(systems::algo::AlgorithmRegistrar& registrar) override
    {
        registrar.addCategory({ "quadrilateral", "四边形网格生成", "", 20 });
        registrar.setLabel("四边形网格生成（示例方法）");
        registrar.setGroup("生成");
        registrar.setOrder(10);
        registrar.setCategoryDefault("quadrilateral", "网格类型", "1");
    }
    // 实现 args_type()、execute()，必要时覆盖 resolveComponentId()。
};
```

- `setup` 每次注册调用一次，查询、分类切换和算法执行不重复调用。默认空实现将算法归入“其他算法”。注册器仅收集声明，不持有模型或界面，不能保存其引用。
- 声明及参数准备成功后整体登记；setup 抛异常时不新增条目，替换失败保留原算法；模型忙碌时在调用 setup 前拒绝注册。
- `addCategory` 接收完整分类描述：稳定 `id`、必填 `title`、可选资源 `icon` 和排序 `order`。`other` 为保留身份。一个算法可以声明多个分类，同一 id 自动合并为同一个按钮和页面，算法身份仍使用各自的 `handler.name`。
- 同类描述应保持一致；冲突时记录警告，选择算法唯一名按字典序最小的完整声明，结果与加载顺序无关。分类按 order、id 排序。
- 三角形、四边形、四面体、六面体四个基础分类始终显示；无算法时显示“暂无可用算法”并禁用执行。扩展分类由当前注册表派生，最后一个提供者卸载后消失。
- `setLabel` 使用业务名称，空时沿用 display_name。`setIcon` 的资源由插件注册；group 为空时使用默认分组，order 越小越靠前，同值按算法身份排序。
- `setCategoryDefault` 按分类设置参数默认值，参数名与 args_type() 一致，Combo 使用选项索引的字符串；进入分类时应用，用户随后可修改。参数控件仍由宿主生成，插件不依赖 app 层。
- 直接 C++ 注册与动态、静态插件统一走 Handler 的 setup；HandlerMetaData 仅保存身份，没有第二套导航声明入口。

此改动更新 AlgorithmHandler 的虚函数接口，旧算法 DLL 不能混用。更新 SDK 后必须重建主程序、项目内插件、示例及独立 Addons 插件，并使用匹配的工具链和依赖。旧 JSON 导航配置和旁置文件应删除；分类需迁移至 setup，不提供兼容解析。
