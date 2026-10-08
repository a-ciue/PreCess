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

`AlgorithmSystem` 插件可在 JSON 的 `handler` 中增加可选 `navigation`，主程序按声明构建导航，插件无需依赖 app 层：

```json
{
  "system": "AlgorithmSystem",
  "handler": {
    "name": "ExampleMesher",
    "display_name": "示例网格生成",
    "navigation": {
      "categories": ["triangle", "tetrahedron"],
      "group": "生成",
      "label": "网格生成（德劳内方法）",
      "icon": "qrc:/example/mesher.svg",
      "order": 10
    }
  }
}
```

- 类别标识固定为 `triangle`（三角形）、`quadrilateral`（四边形）、`tetrahedron`（四面体）、`hexahedron`（六面体）；一个算法可声明多类，同一入口共享既有参数和执行逻辑。
- 未声明导航、类别为空或只有未知类别时，算法进入“其他算法”；包含有效类别时按有效类别展示，不额外进入“其他算法”。
- 顶层“网格生成算法”提供四类入口，具体算法在操作面板选择；“其他算法”保留工具栏按钮。
- `label` 为可选业务名称，例如“三角形网格生成（德劳内方法）”，不填写库名称；必须与算法真实能力一致。未声明时沿用 `display_name`，内部仍按 `name` 分发，不影响脚本或执行接口。
- `group` 为空时归入默认分组；`order` 默认 0，数值越小越靠前，同值按算法唯一名排序。分组顺序由组内最靠前的算法决定。
- `icon` 应由插件自身注册资源；为空时显示通用算法图标。现有 JSON 无需修改即可继续加载。
- 使用 C++ 直接注册时，将同样的信息填入 `HandlerMetaData::navigation`。

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

旁置文件覆盖内嵌的 `handler.navigation`，不会覆盖插件身份或执行接口。文件缺失时使用内嵌声明；文件无法读取、JSON 无效或顶层不是对象时，记录警告并回退内嵌声明，插件仍可正常注册。修改声明后重新加载插件或重启程序生效。静态插件使用内嵌声明。

导航可选 `category_defaults` 按类别声明参数默认值，例如 `"category_defaults": {"triangle": {"网格类型": 0}, "quadrilateral": {"网格类型": 1}}`。键为参数名，Combo 值为选项索引；进入类别时应用，用户随后可调整。通用界面不根据插件名推断参数。

现有 Gmsh / TetGen 二进制插件的业务名称和分类配置见 `algorithm-navigation/`。安装对应插件后，将匹配的 `.navigation.json` 复制到 DLL 同目录，再重新加载插件或重启程序；这些声明仅用于已安装插件，不安装算法二进制。
