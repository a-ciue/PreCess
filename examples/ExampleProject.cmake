# 集合构建与单例构建共用 SDK 发现及安装约定。
include_guard(GLOBAL)
if(NOT PRECESS_PLUGIN_IN_TREE)
    option(BUILD_TESTING "构建 SDK 示例测试" OFF)
    # 必须在外部工程目录作用域启用 CTest，不能只在 SDK 组件解析函数内 include。
    include(CTest)
    find_package(PreCess REQUIRED COMPONENTS Tests)
    set(PRECESS_EXAMPLE_PLUGIN_INSTALL_DIR "plugins" CACHE STRING
        "示例插件安装目录（相对 --prefix；也可指定主程序插件目录的绝对路径）")
endif()
