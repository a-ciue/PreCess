# ExternalPlugin：SDK 独立开发最小骨架

本示例依赖已安装 PreCess SDK 与配套 PreCess-deps，使用 C++20、CMake 3.27+、Ninja。复制时保留上级 ExampleProject.cmake；Windows 先加载 x64 开发环境。

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=<SDK 前缀>;<依赖根>" "-DPreCess_DIR=<SDK 前缀>/lib/cmake/PreCess" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build --prefix "<目标主程序安装目录>" --component AllPlugins
```

- 不固定 SDK 或编译器版本，按 SDK 的 ABI 提示核对依赖和目标主程序；Debug/非 Debug 不混用。
- 插件 DLL/so 在 build/plugins，默认安装到目标前缀的 plugins/；启动主程序后检查插件注册和功能执行。
- 额外依赖先 find_package，再通过 precess_plugin_link_libraries 连接。

本示例的 CMakeLists 同时支持 SDK 独立构建和随主程序构建，独立初始化与安装仅在外部模式执行。两种入口见 [上级说明](../README.md)。插件开发 API 见 SDK 的 skills/zenithgrid-external-plugin-development/SKILL.md。
