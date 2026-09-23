# ExternalPlugin：外部插件独立构建示例

演示脱离 PreCess 源码树、仅经 `find_package(PreCess)` 构建功能插件的最小工程。
设计背景见 `docs/plugin-sdk-design.md`。

## 前提

- 已安装带开发文件的 PreCess（`PRECESS_INSTALL_DEVELOPMENT_FILES=ON`，默认开）：
  安装树含 `include/precess/`、`lib/cmake/PreCess/` 与 `lib/PreCessBase.lib`（Debug 构建为 `PreCessBased.lib`）。
- **同编译器、同配置、同依赖版本**构建（ABI 要求见 AGENTS.md §8；配置期有指纹校验兜底）。

## 构建与安装

```bash
# -DCMAKE_BUILD_TYPE 须与 PreCess 库一致（ABI 校验强制：Debug/Release 混链会堆损坏）；
# 新配置须带 -G Ninja（或你惯用的生成器），并使用与 PreCess 相同的编译器环境
cmake -B build -S . -G Ninja -DCMAKE_PREFIX_PATH=<PreCess 安装前缀> -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build    # 装入 <PreCess 安装前缀>/plugins，启动 PreCess 即被发现
```

**关于 `cmake --install --prefix`**：默认安装目的地是 Config 期解析的**绝对路径**
（PreCess SDK 的 `plugins/`），**不跟随 `--prefix`**（会静默写回原 SDK 目录）。
要让安装跟随 `--prefix`，给 `precess_plugin_install` 传**相对**目的地：

```cmake
precess_plugin_install(ExternalDemoPlugin DESTINATION plugins)  # 相对值按 --prefix 解析
# cmake --install build --prefix <任意目标前缀> → <目标前缀>/plugins/
```

开发期免拷贝调试可直接把 `build/plugins/ExternalDemoPlugin.dll` 拷入 PreCess 的
`plugins/` 目录，或（增强项实现后）经 `PRECESS_PLUGIN_PATH` 指向构建树。

## 注意事项速查（导入 PreCess SDK 检查单）

**硬性要求（ABI 校验配置期强制，错配直接 FATAL）**：

- 同编译器**主.次**版本（如 MSVC 19.44，补丁级可不同）、同 MSVC 工具集、同 C++ 标准；
- `-DCMAKE_BUILD_TYPE` 必须与 PreCess 库一致——Debug/Release 淮链是 `_ITERATOR_DEBUG_LEVEL`
  堆损坏，/MD 与 /MT 淮链是跨模块 new/delete 错配；
- **同一份三方实例**（单实例强约束：Qt · VTK · OCCT · Python）：不要把 `CMAKE_PREFIX_PATH`
  指向另一份 VTK/OCCT/Qt。helpers 会自动引导 6 个 `<pkg>_DIR`（含 `pybind11_DIR`，组件 Python 必需）
  命中 PreCess 构建时的实例，
  版本钉对 FATAL 兜底（VTK/Qt/OCCT，spdlog 为 WARN）；
- 双配置 SDK（多配置生成器单树、分 `--config Debug` / `--config Release` 两次安装到同一前缀）
  两种配置均可直接导入：Debug 库带 `d` 后缀（`PreCessBased.lib`），由 CMake 按
  `CMAKE_BUILD_TYPE` 自动选中，消费者无需其他改动；
- 确认承担风险要强制继续：`-DPRECESS_ALLOW_ABI_MISMATCH=ON`（全部校验降级为警告，不建议）。

**find_package 语义**：

- `find_package(PreCess 0.4 REQUIRED)` 一次获得 `PreCess::Base`（**缺省即导入，无需
  `COMPONENTS`**——Base 恒可用，声明它无增量信息）、
  三方基线（spdlog/OCCT/VTK 组件/Qt6 经 `find_dependency` 自动拉齐）与插件构建 API；
  0.x 阶段版本策略为 SameMinorVersion；
- 组件只服务**可选能力**（命名契约：组件名与导入目标同名）：`Python` ↔ `PreCess::Python`
  （仅真实 Python 构建提供）、`Tests`（Catch2 测试 API，工具组件不产生目标）；
  `Base`（冗余声明）与历史组件 `IO`/`Algo`/`Edit`/`Feature` 均已移除——写入按未知组件报错
  （Base 基础能力裸 `find_package` 即得）；
- 需要 PreCess 已用库的额外组件（如 `find_package(VTK COMPONENTS FiltersGeneral)`）：
  安全，同实例追加组件；
- **许可证边界外**（gmsh GPL / tetgen AGPL / CGAL GPL）：SDK 不提供 Find 模块与路径引导，
  自行 `find_package` 并承担许可义务；gmsh 另需自行解决其对 OCCT 的相对路径硬编码耦合；
- `PRECESS_RELOCATABLE_INSTALL=ON` 的可重定位安装不含路径引导文件：需自行保证三方路径一致
  （**默认 OFF 保本机零配置**——helpers 将 `find_package` 直指本机 deps，主力开发场景免配置）。

**插件结构（最小接入面）**：Handler 实现 + Plugin 类（`Q_PLUGIN_METADATA`，IID
`com.PreCess.systems.<io|algo|edit|feature>.<类名>/1.0`）+ JSON（`system` 路由字段）。
CMake 三步：`precess_add_*_plugin` → `precess_plugin_link_libraries`（私有三方）→
`precess_plugin_install`。私有三方 DLL 由 `precess_plugin_install` 自动收拢
（freetype/spdlog/CRT 除外——由宿主提供）。

**运行与升级**：插件装入 PreCess 的 `plugins/` 即被扫描。运行期暂无 ABI 指纹校验（阶段三
未实施），**构建期校验是唯一防线**——勿轻易绕过。PreCess 升级后请重编插件（0.x 同次版本
CMake 会放行，但 ABI 不保证）；升级 OCCT/VTK 须连带核对 gmsh 与 OCCT VIS（TKIVtk）的对应关系。

## 文件说明

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | `find_package(PreCess)` + `precess_add_feature_plugin` + `precess_plugin_install` |
| `ExternalDemoHandler.{h,cpp}` | 功能实现（继承 `FeatureHandler`） |
| `ExternalDemoPlugin.h` | 插件类（`PluginBase` + `Q_PLUGIN_METADATA`，IID 语法 `com.PreCess.systems.feature.<类名>/1.0`） |
| `ExternalDemoPlugin.json` | 插件元数据（`system` 路由字段 + `handler` 描述） |

## 插件自用第三方依赖

插件需要 PreCess 未用的库时自行 `find_package`。**许可证边界外**的库
（`gmsh` GPL、`tetgen` AGPL、`CGAL` GPLv3）SDK 不提供任何查找引导
（无随包 Find 模块、无 helpers 路径），许可义务由插件开发者自行承担；
需要 **PreCess 已用库**（如 VTK）的额外组件时同样正常 `find_package`，
会命中 PreCess 构建时的同一实例（版本钉对在配置期校验）：

```cmake
find_package(VTK REQUIRED COMPONENTS FiltersGeneral)
precess_add_feature_plugin(...)
precess_plugin_link_libraries(ExternalDemoPlugin VTK::FiltersGeneral)
```
