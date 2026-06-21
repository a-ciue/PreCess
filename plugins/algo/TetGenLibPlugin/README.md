# TetGenLibPlugin：将 TetGen 作为库封装的任务说明

## 当前状态

已将用户提供的 TetGen 源码从 `C:\Users\admin\Downloads\TetGen-main` 复制到本目录：

```text
third_party/tetgen/
├─ tetgen.h
├─ tetgen.cxx
├─ predicates.cxx
├─ CMakeLists.txt
├─ LICENSE
├─ README.md
└─ CHANGELOG.md
```

本目录已经通过 `../CMakeLists.txt` 接入构建，会生成 `TetGenLibPlugin` 插件；TetGen 源码由本插件目录内的 CMake 统一编译为内部静态库 `TetGenInternal`。

## 目标

在 `plugins/algo` 下新增一个基于 TetGen 库调用的算法插件，用库接口替代当前 `TetGenPlugin` 中调用外部 `tetgen` 可执行文件的方案。

旧实现位于 `../TetGenPlugin`，主要流程是：

1. 将当前 component 写出为 Medit `.mesh` 临时文件。
2. 通过 `std::system()` 执行外部 `tetgen` 命令。
3. 读取 TetGen 生成的 `.1.mesh` 文件并导入为新模型。

本插件已改为当前 PreCess 架构下的直接内存转换路线：

1. 从 `ComponentOperator::component()` 获取当前 `ComponentData`。
2. 从 `ComponentOperator::manager().globalPoints()` 还原当前网格顶点坐标。
3. 将 `MeshData::face_vertices_` / `face_vertices_offset_` 转换为 `tetgenio::facetlist`。
4. 直接调用 TetGen 库函数 `tetrahedralize(...)` 完成四面体剖分。
5. 将 TetGen 输出的 `pointlist`、`trifacelist`、`tetrahedronlist` 转换回新的 `MeshData`。
6. 通过 `ModelLayer::addModel(...)` 添加为新模型。

## TetGen 库接口依据

TetGen 手册 1.6 第 6 章说明可以从其他程序调用 TetGen。核心接口包括：

```cpp
#include "tetgen.h"

tetgenio in;
tetgenio out;
tetrahedralize("pq1.414a0.1", &in, &out);
```

其中：

- `tetgen.h` 定义 `tetgenio` 和 `tetrahedralize()`。
- `tetgenio` 用数组替代输入/输出文件，保存点、facet、四面体、边界 marker、孔洞、区域等数据。
- `tetrahedralize()` 的 switches 字符串与命令行参数一致，但不带前导 `-`。

## CMake 集成

当前 `CMakeLists.txt` 会只编译 TetGen 静态库，不编译 `tetgen` 可执行文件：

```cmake
add_library(TetGenInternal STATIC
    third_party/tetgen/tetgen.cxx
    third_party/tetgen/predicates.cxx
)
target_compile_definitions(TetGenInternal PUBLIC TETLIBRARY)
target_include_directories(TetGenInternal PUBLIC third_party/tetgen)
```

注意：TetGen 官方 CMake 中库目标叫 `tet`，并用 `TETLIBRARY` 编译定义启用库模式。

## 当前实现状态

1. 已接入父级 `plugins/algo/CMakeLists.txt`。
2. 已新增 `TetGenLibPlugin.h/.json` 和 `TetGenLibHandler.h/.cpp`。
3. 已实现 `ComponentData -> tetgenio` 输入转换。
4. 已实现 `tetgenio -> MeshData -> ModelLayer::addModel(...)` 输出转换。
5. 已通过复杂网格完成运行验证。
6. 现有 `TetGenPlugin` 可继续作为命令行 fallback 或调试对照。

## 后续可扩展项

1. 检查 TetGen 许可证与 PreCess 插件许可证边界是否兼容。
2. 补充局部尺寸、孔洞、区域属性等 TetGen 参数。
3. 保留并传递 face marker、region attribute、material 等属性。
4. 增强输入网格诊断，例如开口、非流形边、退化三角形。
5. 改进自交检测结果的 UI 提示。
6. 改进结果模型命名。

## 已验证关键点（2025-06-21）

- 库调用已通过复杂网格验证，可生成表面三角面和四面体体网格。
- **参数极简原则**：库模式下应避免直接沿用命令行插件中的文件输出控制参数（如 `B`、`N`、`E`、`F`、`V`、`Y`、`A`），否则可能触发 TetGen 内部空指针异常。
- 当前参数集：
  - `p`：读取 PLC（piecewise linear complex）
  - `q<value>`：质量控制，如 `q1.2`，`0` 表示关闭
  - `a<value>`：最大四面体体积，`0` 表示关闭
  - `Y`：保留原始表面（可选）
  - `H`：仅保留最外层腔体（可选）
  - `d`：仅检测 PLC 自交（可选，启用时不生成结果网格）
  - `Q`：静默模式，减少控制台输出
- 当前 UI 默认值：
  - `是否仅保留最外层腔体`：否
  - `质量参数 q`：`1.2`
  - `最大单元体积 a`：`0`（关闭）
  - `是否保留原始表面`：否
  - `是否仅检测自交`：否
- `Combo` 参数内容支持 `选项1,选项2|默认索引` 格式，例如 `是,否|1` 表示默认选择“否”。
- 结果通过 `ModelLayer::addModel(...)` 直接加入项目，不再绕 Medit IO 临时文件。
- 输入数据直接从 `context.cur_component.component().mesh` 和 `context.cur_component.manager().globalPoints()` 构造。

## 外部预编译库的缺点

外部预编译库指直接拿已有的 `tetgen.lib`、`tetgen.dll`、`libtetgen.a` 或 `libtetgen.so` 来链接，而不是随项目源码统一编译。

主要缺点：

1. **ABI 兼容风险**
   - C++ 库的 ABI 受编译器、标准库、运行时、编译选项影响。
   - Windows 下尤其需要匹配 MSVC 版本、Debug/Release、运行时库 `/MD` 或 `/MT`。
   - 如果 TetGen 库和 PreCess 使用的编译环境不一致，可能出现链接失败、运行时崩溃或内存释放错误。

2. **跨平台维护成本高**
   - Windows、Linux、macOS 都需要分别准备对应二进制。
   - 不同架构如 x64、arm64 也要单独维护。
   - CI/CD、安装包和用户本地构建都需要处理库搜索路径和运行时部署。

3. **Debug/Release 不一致问题**
   - Debug 版程序链接 Release 版 C++ 库时，标准库对象、内存分配和断言行为可能不一致。
   - Windows 下 Debug CRT 与 Release CRT 混用风险更高。

4. **依赖和部署复杂**
   - 动态库方案需要确保运行时能找到 `tetgen.dll` / `libtetgen.so`。
   - 安装包需要额外复制库文件，并处理 PATH、rpath 或 install_name。
   - 静态库方案虽然部署简单，但编译选项和许可证义务需要更明确。

5. **版本不可控**
   - 用户或开发者机器上可能存在不同版本 TetGen。
   - 同样的 PreCess 代码链接到不同 TetGen 版本，行为可能不同，导致 bug 难复现。

6. **源码级调试困难**
   - 没有源码或调试符号时，很难排查 TetGen 内部错误。
   - 对输入数据非法、边界条件、异常退出等问题定位会更慢。

7. **CMake 集成不稳定**
   - 如果预编译库没有规范的 `TetGenConfig.cmake`，需要手写 `find_library()` / `find_path()`。
   - 用户需要手动设置 `TETGEN_ROOT`、`TETGEN_LIBRARY`、`TETGEN_INCLUDE_DIR` 等变量。

8. **许可证和分发边界更容易被忽略**
   - 预编译二进制随安装包分发时，需要确认 TetGen 许可证是否允许该分发方式。
   - 如果项目区分 GPL/LGPL 插件边界，静态/动态链接方式也需要重新评估。

## 更推荐的方向

优先使用当前目录中的 TetGen 源码，由 PreCess 的 CMake 统一编译成内部静态库。这种方式更容易保证编译器、运行时、构建类型和部署方式一致，也更适合长期维护。
