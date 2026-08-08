# TetGenLibPlugin

基于 [TetGen](http://wias-berlin.de/software/tetgen/) 库接口的四面体网格剖分算法插件。
通过内存直接转换完成剖分，无需临时文件，替代旧版 `TetGenPlugin` 中调用外部 `tetgen` 可执行文件的方案。

## 目录结构

```text
TetGenLibPlugin/
├── CMakeLists.txt
├── TetGenLibPlugin.h          # 插件入口，注册 TetGenLibHandler
├── TetGenLibPlugin.json       # 插件元数据（注册到 AlgorithmSystem）
├── TetGenLibHandler.h/.cpp    # 算法处理器实现
└── test/
    ├── CMakeLists.txt
    └── TestTetGenLibHandler.cpp
```

> TetGen 库不再随插件源码分发，改为通过 `find_package(tetgen)` 消费预编译产物。
> 依赖构建由项目根目录的 `PreCess-deps.bat` 统一处理，产物安装到
> `${深度目录}/tetgen1.6.0/`（包含 `include/tetgen.h` 与 `lib/tet.lib`）。

## 功能

对当前选中的面网格组件执行四面体剖分，生成包含三角表面和四面体体的新模型，直接加入当前项目。

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 是否仅使用最大表面壳 | Combo（是/否） | 否 | 开启后通过 BFS 连通分析只取面数最多的表面壳，过滤孤立小腔体 |
| 质量参数 q | Float | 1.2 | TetGen 质量约束（半径/边长比上限），设为 0 关闭 |
| 最大单元体积 a | Float | 0 | 限制四面体最大体积，设为 0 关闭 |
| 是否保留原始表面 | Combo（是/否） | 否 | 对应 TetGen `-Y`，剖分后保留输入表面网格不变 |
| 是否仅检测自交 | Combo（是/否） | 否 | 对应 TetGen `-d`，仅检测 PLC 自交并输出日志，不生成结果网格 |

### 结果模型命名

结果模型名自动拼接参数信息，格式如：
```
原模型名_TetGen_q1.2_a0.1_Y_largestShell
```

## 处理流程

1. 从 `ComponentOperator::component()` 获取当前 `ComponentData`
2. 直读 `MeshData::vertex_positions_` 顶点坐标（MeshData 自包含，连通性数组存组件内局部点 id）
3. 将 `MeshData::face_vertices_` / `face_vertices_offset_` 转换为 `tetgenio::facetlist`
4. 调用 `tetrahedralize(switches, &in, &out)` 完成剖分
5. 将 TetGen 输出的 `pointlist`、`trifacelist`、`tetrahedronlist` 转回 `MeshData`
6. 通过 `ModelLayer::addModel()` 添加为新模型，或替换当前 Component 的 mesh（替换后调 `ComponentData::ensurePointGlobalIds` 补分配全局点 id）

## 与 TetGenPlugin 的区别

| 对比项 | TetGenPlugin（旧） | TetGenLibPlugin（本插件） |
|--------|-------------------|-------------------------|
| 调用方式 | `std::system()` 执行外部 `tetgen` 命令 | 直接调用 `tetrahedralize()` 库函数 |
| 数据交换 | 写出 Medit `.mesh` 临时文件再读回 | 内存直接转换，无临时文件 |
| 部署依赖 | 需 PATH 中能找到 `tetgen` 可执行文件 | 通过 `find_package(tetgen)` 链接预编译静态库 |

`TetGenPlugin` 可继续作为命令行 fallback 或调试对照使用。

## CMake 集成

TetGen 由项目根 `PreCess-deps.bat` 预编译到依赖目录，插件通过 `find_package` 消费。
仓库中的 `cmake/Findtetgen.cmake` 负责在 `CMAKE_PREFIX_PATH` 下定位 TetGen 的头文件与静态库，
并暴露 imported target `tetgen::tetgen`（已附带 `TETLIBRARY` 编译定义，使 TetGen 以库模式使用）：

```cmake
find_package(tetgen REQUIRED)

precess_add_algo_plugin(TetGenLibPlugin
    SOURCES "TetGenLibHandler.cpp"
    PLUGIN_H "TetGenLibPlugin.h"
)
precess_plugin_link_libraries(TetGenLibPlugin tetgen::tetgen)

add_subdirectory(test)
```

## 已知限制

- **参数极简原则**：库模式下应避免使用命令行插件中的文件输出控制参数（`B`、`N`、`E`、`F`、`V`、`Y`、`A`），否则可能触发 TetGen 内部空指针异常。
- **`-H`（孔洞文件）未集成**：TetGen 1.6 的 `-H` 需要额外 hole mesh 文件路径，只追加 `H` 而不提供路径会导致访问冲突。
- **许可证**：TetGen 采用 **AGPLv3** 许可，因此本插件及 PreCess 的 `app/` / `plugins/` 层同样按 **AGPLv3** 发布；`core/` 与 `model/` 仍保持 **LGPLv3**，二者兼容。基于本插件或打包了本插件的 PreCess 发行版分发衍生软件时，须遵守 AGPLv3（含第 13 条网络分发条款）。
- **属性传递**：当前未传递 face marker、region attribute、material 等属性。
