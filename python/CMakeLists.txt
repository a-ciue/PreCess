# precess Python 绑定模块与内嵌解释器宿主（LGPLv3）
#
# 产物：
# - precess 模块（pyd）：会话层绑定，需 Python3(Interpreter/Module) + pybind11
# - precess_runtime 静态库（无 Qt）：内嵌解释器宿主，目标恒存在——Python3 +
#   pybind11 可用且非 wasm 时编译 runtime.cpp（真实现），否则编译 stub.cpp
#   （桩：available 恒 false、错误信息说明原因），宿主（GUI/CLI）无需条件编译

option(PRECESS_BUILD_PYTHON
    "构建 precess 真实 Python 绑定与内嵌解释器（OFF 或依赖缺失时 precess_runtime 自动降级为桩实现）" ON)

# 解释器选择：默认由 FindPython 在系统 PATH 等常规位置发现；经环境变量
# PRECESS_PYTHON_PATH 指定 python.exe 所在目录时优先采用（该变量同时前置
# 进运行时 PATH，保证构建与运行使用同一解释器，避免混用）
file(TO_CMAKE_PATH "$ENV{PRECESS_PYTHON_PATH}" _precess_python_dir)
if(_precess_python_dir AND EXISTS "${_precess_python_dir}/python.exe")
    set(Python3_EXECUTABLE "${_precess_python_dir}/python.exe"
        CACHE FILEPATH "构建与运行所用 Python 解释器（经 PRECESS_PYTHON_PATH 指定）" FORCE)
endif()
unset(_precess_python_dir)

# Development.Embed（内嵌宿主需要）：须在本文件内、find_package(pybind11) 之前
# 一并请求——pybind11::embed 仅在 Python3::Python 目标已存在时才把 Python 链接
# 接口挂入自身，时序颠倒会导致宿主消费者缺库
find_package(Python3 COMPONENTS Interpreter Development.Module Development.Embed QUIET)

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
        find_package(pybind11 CONFIG QUIET HINTS "${_pybind11_dir}")
    endif()
    if(NOT pybind11_FOUND)
        message(STATUS
            "未找到 pybind11，precess_runtime 降级为桩模式（运行时 Python 功能不可用）；"
            "安装请经 PreCess-deps.py 或 pip install pybind11")
        set(_precess_real OFF)
    endif()
endif()

# 导出构建所用解释器目录（标准库根/python313.dll 所在），运行时宿主配置
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

    # 安装规则（安装树自包含）：precess 扩展模块装到 <exe_dir>/python（运行时
    # 宿主的首选模块目录候选）；随包分发构建所用解释器的标准库（<exe_dir>/Lib）
    # 与二进制扩展模块（<exe_dir>/DLLs，_socket 等非内建扩展），python313.dll
    # 本体已由 app 的依赖收拢放入 <exe_dir>——三者凑齐即运行期探测可就地定位
    # 标准库（宿主 python_home 探测顺序见 QPythonRuntime::makeRuntimeConfig）。
    # 排除纯开发/测试内容控制体积；解释器非标准布局（无 Lib 目录）时跳过并提示
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
