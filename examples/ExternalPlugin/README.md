# ExternalPlugin：外部插件独立构建示例

演示脱离 PreCess 源码树、仅经 `find_package(PreCess)` 构建功能插件的最小工程。
设计背景见 `docs/plugin-sdk-design.md`。

## 前提

- 已安装带开发文件的 PreCess（`PRECESS_INSTALL_DEVELOPMENT_FILES=ON`，默认开）：
  安装树含 `include/precess/`、`lib/cmake/PreCess/` 与 `lib/PreCessBase.lib`。
- **同编译器、同配置、同依赖版本**构建（ABI 要求见 AGENTS.md §8；配置期有指纹校验兜底）。

## 构建与安装

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=<PreCess 安装前缀>   # 例：D:/PreCess
cmake --build build
cmake --install build    # 装入 <PreCess 安装前缀>/plugins，启动 PreCess 即被发现
```

开发期免拷贝调试可直接把 `build/plugins/ExternalDemoPlugin.dll` 拷入 PreCess 的
`plugins/` 目录，或（增强项实现后）经 `PRECESS_PLUGIN_PATH` 指向构建树。

## 文件说明

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | `find_package(PreCess)` + `precess_add_feature_plugin` + `precess_plugin_install` |
| `ExternalDemoHandler.{h,cpp}` | 功能实现（继承 `FeatureHandler`） |
| `ExternalDemoPlugin.h` | 插件类（`PluginBase` + `Q_PLUGIN_METADATA`，IID 语法 `com.PreCess.systems.feature.<类名>/1.0`） |
| `ExternalDemoPlugin.json` | 插件元数据（`system` 路由字段 + `handler` 描述） |

## 插件自用第三方依赖

插件需要 PreCess 未用的库（如 `gmsh`、`tetgen`）时自行 `find_package`；
需要 **PreCess 已用库**（如 VTK）的额外组件时同样正常 `find_package`，
会命中 PreCess 构建时的同一实例（版本钉对在配置期校验）：

```cmake
find_package(VTK REQUIRED COMPONENTS FiltersGeneral)
precess_add_feature_plugin(...)
precess_plugin_link_libraries(ExternalDemoPlugin VTK::FiltersGeneral)
```
