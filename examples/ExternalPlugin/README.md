# ExternalPlugin —— 用 PreCess SDK 构建插件 DLL

本目录是一个完整的独立 CMake 工程（随 SDK 安装在 `<SDK 前缀>/examples/ExternalPlugin/`），
演示如何仅凭 SDK 与第三方依赖编译出可被 PreCess 主程序加载的插件 DLL。

## 准备

1. **PreCess SDK**：本示例所在前缀，含头文件与 `find_package(PreCess)` 包配置。
2. **第三方依赖（不随 SDK 安装）**：从 PreCess 项目**发行页**下载与本 SDK 版本配套的
   预编译依赖包 `PreCess-deps`（Qt6、VTK、OpenCASCADE、spdlog、freetype 等），解压到任意目录。
   插件链接 `PreCess::Base` 时需要这些依赖的头文件与导入库。
3. **工具链**：与构建本 SDK 版本兼容的 MSVC 工具集；构建配置按 **Debug / 非 Debug 成类**：
   Debug 插件只能配 Debug 主程序（混类链接期即失败，强行绕过运行期堆损坏）；
   同为非 Debug 时 `Release` 与 `RelWithDebInfo`/`MinSizeRel` 互相兼容——
   插件开发想带调试信息编译，用 `-DCMAKE_BUILD_TYPE=RelWithDebInfo` 即可。

## 编译

Windows 环境下需要先下载Visual Studio并安装C++开发工具。
然后打开“x64 Native Tools Command Prompt/PowerShell”或“Developer Command Prompt/PowerShell for VS”启用开发环境，并切换到本目录。

在本目录执行：

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=<SDK 前缀>;<PreCess-deps 根>" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

`CMAKE_PREFIX_PATH` 用分号给出**两根**：本 SDK 的前缀，以及发行页下载的 `PreCess-deps` 解压根。
CMake 在这两根下即可找到 `PreCess` 包与全部依赖，无需其他参数。

## 安装到 PreCess 并加载

可以在软件中直接加载插件，也可以通过下面的命令将插件安装到 PreCess 的 `plugins` 目录下，启动 PreCess 即自动发现：

```powershell
cmake --install build --prefix <PreCess 安装目录>
```

该命令把 `ExternalDemoPlugin.dll/so` 装入 `<PreCess 安装目录>/plugins/`。也可手动复制 
`build/ExternalDemoPlugin.dll/so` 到同一位置——启动 PreCess 即自动发现。

## 使用更多依赖组件

插件默认只依赖 PreCess 自身的接口。若要用额外三方组件（例如特定 VTK 模块），在
`CMakeLists.txt` 中取消对应 `find_package(VTK …)` 与 `precess_plugin_link_libraries(...)` 的
注释行即可，依赖根仍在上面的 `CMAKE_PREFIX_PATH` 中。
