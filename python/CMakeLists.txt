# precess Python 绑定模块与内嵌解释器宿主（LGPLv3）
#
# 产物：
# - precess 模块（pyd）：会话层绑定，需 Python3(Interpreter/Module) + pybind11
# - precess_runtime 静态库（无 Qt）：内嵌解释器宿主，目标恒存在——Python3 +
#   pybind11 可用且非 wasm 时编译 runtime.cpp（真实现），否则编译 stub.cpp
#   （桩：available 恒 false、错误信息说明原因），宿主（GUI/CLI）无需条件编译

option(PRECESS_BUILD_PYTHON
    "构建 precess 真实 Python 绑定与内嵌解释器（OFF 或依赖缺失时 precess_runtime 自动降级为桩实现）" ON)

# 解释器选择：默认由 FindPython 经系统 PATH 发现；经可选缓存变量指定时转交
# Python3_ROOT_DIR（平台命名差异由 FindPython 处理），并清掉粘性的解释器
# 缓存使新目录生效
set(PRECESS_PYTHON_PATH "" CACHE PATH
    "构建所用 Python 解释器目录（可选；留空用系统 PATH 发现）")
if(PRECESS_PYTHON_PATH)
    set(Python3_ROOT_DIR "${PRECESS_PYTHON_PATH}" CACHE PATH
        "FindPython 解释器发现根目录（经 PRECESS_PYTHON_PATH 指定）" FORCE)
    unset(Python3_EXECUTABLE CACHE)
endif()

# Development.Embed（内嵌宿主需要）：须在本文件内、find_package(pybind11) 之前
# 一并请求——pybind11::embed 仅在 Python3::Python 目标已存在时才把 Python 链接
# 接口挂入自身，时序颠倒会导致宿主消费者缺库
find_package(Python3 COMPONENTS Interpreter Development.Module Development.Embed QUIET)

if(NOT Python3_Interpreter_FOUND)
    message(WARNING
        "请确认 Python 被正确加入环境变量中，或者手动指定解释器所在目录 PRECESS_PYTHON_PATH=\"${PRECESS_PYTHON_PATH}\"")
endif()

set(_precess_real OFF)
if(PRECESS_BUILD_PYTHON AND Python3_FOUND AND NOT EMSCRIPTEN
        AND Python3_Development.Module_FOUND AND Python3_Development.Embed_FOUND)
    set(_precess_real ON)
endif()

if(_precess_real)
    # pybind11 查找顺序：依赖包提供（CMAKE_PREFIX_PATH）→ 本机 pip 安装包
    # （python -m pybind11 --cmakedir 定位）；均未命中时降级为桩模式（不再
    # 配置失败——precess_runtime 恒存在，缺失原因经运行时错误信息可见）
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
            OUTPUT_VARIABLE _pybind11_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _pybind11_result
        )
        # pybind11 3.1 起 --cmakedir 对含空格的路径会输出带双引号的路径，
        # 去引号后再交给 find_package，否则 HINTS 解析失败误判未找到
        string(REPLACE "\"" "" _pybind11_dir "${_pybind11_dir}")
        find_package(pybind11 CONFIG QUIET HINTS "${_pybind11_dir}")
    endif()
    if(NOT pybind11_FOUND)
        message(STATUS
            "未找到 pybind11，precess_runtime 降级为桩模式（运行时 Python 功能不可用）；"
            "安装请经 PreCess-deps.py 或 pip install pybind11")
        set(_precess_real OFF)
    endif()
endif()

# 导出构建所用解释器目录（标准库根/解释器运行库所在），运行时宿主配置
# 与 app 安装依赖收拢共用（跨目录可见的缓存变量；桩模式下为空）
if(_precess_real)
    get_filename_component(PRECESS_PYTHON_HOME_DIR "${Python3_EXECUTABLE}" DIRECTORY)
else()
    set(PRECESS_PYTHON_HOME_DIR "")
endif()
# FORCE：set(CACHE) 对已存在条目默认不覆盖——依赖发现失败的某次配置写入
# 空值/旧值后会永久粘住（含 CMake 自动 re-configure 的第二遍），必须可写回
set(PRECESS_PYTHON_HOME_DIR "${PRECESS_PYTHON_HOME_DIR}" CACHE INTERNAL
    "构建所用 Python 解释器目录（标准库根；桩模式为空）" FORCE)

# 内嵌解释器宿主（静态库，无 Qt）：目标恒存在，真实现/桩按可用性二选一
add_library(precess_runtime STATIC)
target_include_directories(precess_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(precess_runtime PUBLIC Session)
if(_precess_real)
    target_sources(precess_runtime PRIVATE runtime.cpp)
    target_link_libraries(precess_runtime PRIVATE pybind11::embed spdlog::spdlog)
else()
    target_sources(precess_runtime PRIVATE stub.cpp)
endif()

if(_precess_real)
    # CMake 目标名取 precess_bindings 与主程序 PreCess 区分；Python 导入名
    # 保持 precess 不变（经 OUTPUT_NAME 固定输出文件名）
    pybind11_add_module(precess_bindings precess_module.cpp)
    set_target_properties(precess_bindings PROPERTIES OUTPUT_NAME "precess")
    target_link_libraries(precess_bindings PRIVATE
        Session
    )

    # 构建树自包含（仅 Windows）：把解释器运行库（版本相关，经导入目标求值）
    # 拷到可执行文件目录（Qt 工程运行时产物统一在构建根目录），运行/调试不再
    # 依赖 PATH 中的 Python；类 Unix 由 rpath 解析 libpython，无需拷贝
    if(WIN32)
        add_custom_command(TARGET precess_bindings POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Python3::Python>"
                "${CMAKE_BINARY_DIR}"
        )
    endif()

    if (BUILD_TESTING)
        add_test(NAME precess_python_smoke
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/test/test_precess.py" --plugins "${CMAKE_BINARY_DIR}/plugins")
        # Python 3.8 起扩展模块的依赖 DLL 不再经 PATH 解析，须经脚本侧
        # os.add_dll_directory 注册：DLL 目录清单经 PRECESS_DLL_DIRS 传递
        set_tests_properties(precess_python_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:precess_bindings>"
            ENVIRONMENT_MODIFICATION "$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess_bindings>,PREPEND,PATH=path_list_prepend:>;$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess_bindings>,PREPEND,PRECESS_DLL_DIRS=path_list_append:>")
        # C++ 嵌入测试：验证宿主初始化、控制台执行与活会话调用（依赖真实
        # 宿主与 precess 模块，桩模式无意义）
        precess_add_test(TestPrecessRuntime test/test_runtime.cpp)
        target_link_libraries(TestPrecessRuntime PRIVATE precess_runtime)
        target_compile_definitions(TestPrecessRuntime PRIVATE
            PRECESS_PYTHON_HOME="${PRECESS_PYTHON_HOME_DIR}"
            PRECESS_PYTHON_MODULE_DIR="$<TARGET_FILE_DIR:precess_bindings>"
            PRECESS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins")
    endif()

    # 安装树自包含：pyd 装到宿主首选模块目录 <exe_dir>/python，解释器标准库
    # 与二进制扩展随包分发到 exe 目录（运行库本身由 app 依赖收拢放入）；
    # 排除开发/测试内容控制体积，非标准布局（无 Lib/）时跳过
    install(TARGETS precess_bindings
        LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/python"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/python"
    )
    if(EXISTS "${PRECESS_PYTHON_HOME_DIR}/Lib")
        install(DIRECTORY "${PRECESS_PYTHON_HOME_DIR}/Lib"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "site-packages" EXCLUDE
            PATTERN "idlelib" EXCLUDE
            PATTERN "tkinter" EXCLUDE
            PATTERN "tcl" EXCLUDE
            PATTERN "test" EXCLUDE
            PATTERN "tests" EXCLUDE
        )
        if(EXISTS "${PRECESS_PYTHON_HOME_DIR}/DLLs")
            install(DIRECTORY "${PRECESS_PYTHON_HOME_DIR}/DLLs"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
                PATTERN "__pycache__" EXCLUDE
            )
        endif()
    else()
        message(STATUS
            "解释器目录 ${PRECESS_PYTHON_HOME_DIR} 下无标准库 Lib/，安装树将不随包分发 Python 标准库（依赖目标机器的 Python 安装）")
    endif()
endif()
