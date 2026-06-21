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

本目录目前仍是任务/实验目录，尚未在 `../CMakeLists.txt` 中 `add_subdirectory`，不会影响现有构建。

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

## CMake 草案

已新增 `CMakeLists.txt.draft`，记录未来接入方式。核心思路是只编译 TetGen 静态库，不编译 `tetgen` 可执行文件：

```cmake
add_library(TetGenInternal STATIC
    third_party/tetgen/tetgen.cxx
    third_party/tetgen/predicates.cxx
)
target_compile_definitions(TetGenInternal PUBLIC TETLIBRARY)
target_include_directories(TetGenInternal PUBLIC third_party/tetgen)
```

注意：TetGen 官方 CMake 中库目标叫 `tet`，并用 `TETLIBRARY` 编译定义启用库模式。

## 初步实施步骤

1. 检查 TetGen 许可证与 PreCess 插件许可证边界是否兼容。
2. 将 `CMakeLists.txt.draft` 转为正式 `CMakeLists.txt`，并决定是否接入父级 `plugins/algo/CMakeLists.txt`。
3. 新建 `TetGenLibPlugin.h/.json` 和 `TetGenLibHandler.h/.cpp`。
4. 当前已实现 `ComponentData -> tetgenio` 输入转换。
5. 当前已实现 `tetgenio -> MeshData -> ModelLayer::addModel(...)` 输出转换。
6. 后续需要用简单封闭测试网格验证运行稳定性。
7. 后续可继续补充区域、孔洞、局部尺寸、属性和错误诊断。
8. 保留现有 `TetGenPlugin` 作为命令行 fallback 或调试对照。

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
