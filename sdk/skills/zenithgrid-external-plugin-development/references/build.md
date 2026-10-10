# SDK 独立插件构建与分发

## 工程

需要已安装 SDK、配套 PreCess-deps、CMake 3.27+、Ninja、C++20 工具链。Windows 先加载所选工具链的 x64 开发环境；按 SDK ABI 提示核对，Debug/非 Debug 不混用。不在 find_package、脚本或预设中限定 SDK/编译器版本。

```cmake
cmake_minimum_required(VERSION 3.27)
project(MyFeaturePlugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTING "构建插件测试" OFF)
include(CTest)
find_package(PreCess REQUIRED COMPONENTS Tests)

precess_add_feature_plugin(MyFeaturePlugin
    SOURCES MyFeatureHandler.cpp
    PLUGIN_H MyFeaturePlugin.h
)
precess_plugin_install(MyFeaturePlugin DESTINATION plugins)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
```

- 按系统选择 precess_add_io_plugin / precess_add_edit_plugin / precess_add_feature_plugin；新增算法使用 precess_add_feature_plugin。precess_add_algo_plugin 已废弃并冻结，仅供已有旧插件维护；生成动态插件和供测试链接的 <目标名>lib。独立工程不使用 STATIC。
- 额外依赖先 find_package，再用 precess_plugin_link_libraries 连接；VTK 等基线依赖使用 SDK 配套实例，GPL 依赖由插件工程维护。
- 测试用 Tests 组件、precess_add_test 和 precess_test_link_libraries，连接 <目标名>lib 与 PreCess::Base，不连接 Data、Session 等源码树目标。
- 复用 examples 时保留 ExampleProject.cmake，不手动设置 PRECESS_PLUGIN_IN_TREE。更新 SDK/公共头后重建全部插件；更换 SDK 或配置时使用新的构建目录。

## 构建、安装、加载

```powershell
cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=<SDK 前缀或目标主程序安装目录>;<PreCess-deps 根>" "-DPreCess_DIR=<SDK 前缀>/lib/cmake/PreCess" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix "<目标主程序安装目录>" --component AllPlugins
```

- 动态插件输出到 build/plugins。上面显式指定相对 DESTINATION plugins，安装到目标前缀的 plugins/；macOS 使用实际 .app/Contents/plugins。
- 未传 DESTINATION 时，SDK 默认目录按 SDK 前缀解析，通常为绝对路径，--prefix 无法将其改指另一套主程序。安装前核对目标路径，不依赖 -DPRECESS_PLUGIN_INSTALL_DIR 覆盖默认值。
- 在匹配主程序中检查注册、功能执行和 undo/redo；后台功能再检查取消与终态。必要时补齐配套 DLL 搜索路径。仅构建和测试成功不能宣称加载验证通过。
- CPack 使用相对 DESTINATION，绝对目录会绕过 staging。插件自身 project(VERSION ...) 是发布版本，与 SDK 版本分开维护。
