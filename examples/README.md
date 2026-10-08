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

新算法插件推荐在 Handler 的 `setup` 中声明导航，同时继承可选的 `AlgorithmSetup` 接口。JSON 只需保留 `system` 和 `handler` 的身份信息，不必写导航配置：

```cpp
#include "AlgorithmHandler.h"
#include "AlgorithmRegistrar.h"
#include "AlgorithmSetup.h"

class ExampleQuadMesher : public systems::algo::AlgorithmHandler,
                          public systems::algo::AlgorithmSetup {
public:
    void setup(systems::algo::AlgorithmRegistrar& registrar) override
    {
        registrar.addCategory({ "quadrilateral", "四边形网格生成", "", 20 });
        registrar.setLabel("四边形网格生成（示例方法）");
        registrar.setGroup("生成");
        registrar.setOrder(10);
        registrar.setCategoryDefault("quadrilateral", "网格类型", "1");
    }
    // 按既有接口实现 args_type()、execute()，必要时覆盖 resolveComponentId()。
};
```

`setup` 在每次注册时调用一次；普通查询、切换分类或执行算法不会重复调用。注册器仅收集声明，不持有模型或界面，不得保存其引用。完整声明经系统统一校验后登记；setup 抛异常时不添加条目，替换失败时保留原注册。忙碌时在调用 setup 前拒绝注册。

实现 `AlgorithmSetup` 后，其声明整体优先于 JSON 和旁置文件，包括空声明，避免不同来源拼接出不一致配置。未实现该接口的旧插件继续走下面的 JSON / 旁置文件路径。原 `AlgorithmHandler` 虚函数布局保持不变；这不免除 SDK 版本、编译器和运行库的一般 ABI 兼容要求。

兼容路径：旧插件可在 JSON 的 `handler` 中增加可选 `navigation`，主程序按声明构建导航，插件无需依赖 app 层：

```json
{
  "system": "AlgorithmSystem",
  "handler": {
    "name": "ExampleQuadMesher",
    "display_name": "示例网格生成",
    "navigation": {
      "categories": [
        {
          "id": "quadrilateral",
          "title": "四边形网格生成",
          "icon": "qrc:/example/quad.svg",
          "order": 20
        }
      ],
      "group": "生成",
      "label": "四边形网格生成（示例方法）",
      "icon": "qrc:/example/mesher.svg",
      "order": 10
    }
  }
}
```

- 二级分类由已注册算法的声明自动生成，宿主不维护固定按钮列表。插件可复用分类，也可声明新的分类；一个算法可以归属多个分类。
- 分类对象的 `id` 是稳定身份，`title` 是必填显示名称，`icon` 可省略并使用通用图标，`order` 默认 0。`other` 是保留身份，不得声明为分类。
- 不同插件使用相同分类 `id` 时共用一个按钮及页面；显示名称相同但 `id` 不同时不合并。具体算法仍使用独立的 `handler.name`，同名算法注册保持既有替换语义。
- 相同分类的显式声明应一致。存在冲突时记录警告，选择算法唯一名按字典序最小的完整声明；显式对象优先于旧字符串的默认描述。分类按 `order` 排序，同值按 `id` 排序，结果与加载顺序无关。
- 兼容旧字符串 `triangle`（三角形）、`quadrilateral`（四边形）、`tetrahedron`（四面体）、`hexahedron`（六面体），它们集中补齐默认名称、图标和排序。未知旧字符串继续忽略，自定义分类须使用对象声明。
- 未声明有效分类的算法进入“其他算法”；声明有效分类的算法只进入对应分类页面。“其他算法”保留工具栏按钮，具体网格算法在操作面板选择。
- 卸载最后一个提供者后分类按钮消失；当前类别消失时清理活动操作，当前算法退出但类别仍在时选择剩余可用算法。无关插件变化或分类改名不会清空当前参数。
- `label` 为可选业务名称，例如“三角形网格生成（德劳内方法）”，不填写库名称；必须与算法真实能力一致。未声明时沿用 `display_name`，内部仍按 `name` 分发，不影响脚本或执行接口。
- `group` 为空时归入默认分组；`order` 默认 0，数值越小越靠前，同值按算法唯一名排序。分组顺序由组内最靠前的算法决定。
- `icon` 应由插件自身注册资源；为空时显示通用算法图标。现有 JSON 无需修改即可继续加载。
- 使用 C++ 直接注册时，将分类身份填入 `HandlerMetaData::navigation.categories`；自定义分类的完整描述填入 `category_definitions`，其身份也会自动加入所属类别。分类表从算法注册表派生，不需要单独注册或注销按钮。

对于已安装且没有源码的算法 DLL，可在 DLL 旁放置同名 `.navigation.json`，例如 `ExampleMesher.dll` 对应 `ExampleMesher.navigation.json`：

```json
{
  "categories": ["tetrahedron"],
  "label": "四面体网格生成（德劳内方法）",
  "group": "生成",
  "icon": "qrc:/example/mesher.svg",
  "order": 10
}
```

未实现 `AlgorithmSetup` 时，旁置文件覆盖内嵌的 `handler.navigation`，不会覆盖插件身份或执行接口。文件缺失时使用内嵌声明；文件无法读取、JSON 无效或顶层不是对象时，记录警告并回退内嵌声明，插件仍可正常注册。修改声明后重新加载插件或重启程序生效。静态插件使用内嵌声明。

导航可选 `category_defaults` 按类别声明参数默认值，例如 `"category_defaults": {"triangle": {"网格类型": 0}, "quadrilateral": {"网格类型": 1}}`。键为参数名，Combo 值为选项索引；进入类别时应用，用户随后可调整。通用界面不根据插件名推断参数。

现有 Gmsh / TetGen 二进制插件的业务名称和分类配置见 `algorithm-navigation/`。安装对应插件后，将匹配的 `.navigation.json` 复制到 DLL 同目录，再重新加载插件或重启程序；这些声明仅用于已安装插件，不安装算法二进制。

两个外部插件共享分类时，在各自 JSON 中使用相同的分类对象、不同的 `handler.name`；例如 `ExampleQuadMesherA` 和 `ExampleQuadMesherB` 都声明 `quadrilateral`，页面内显示两个算法选项。新增 `polyhedral` 等类别只需提供完整对象，不需要修改主程序 QML。

JSON 格式兼容不等于 DLL ABI 兼容。修改 SDK 共享头文件后需全量重建主程序及项目内插件；独立插件仍须使用与目标主程序兼容的 SDK 和构建配置。
