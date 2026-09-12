# precess Python 绑定模块与内嵌解释器宿主（LGPLv3）
#
# pybind11 优先经依赖包提供（CMAKE_PREFIX_PATH），其次本机 pip 安装包
# （python -m pybind11 --cmakedir 定位）；均未命中时配置失败。
# Python 不可达时整目录静默跳过。产物：
# - precess 模块（pyd）：会话层绑定，需 Development.Module
# - precess_runtime 静态库（无 Qt）：内嵌解释器宿主，供 GUI 的 QPythonRuntime
#   与无头 CLI 复用，需 Development.Embed（缺失时仅跳过宿主目标）

set(PRECESS_BUILD_PYTHON ON CACHE BOOL "构建 Python 绑定模块 precess")

# 解释器选择：默认由 FindPython 在系统 PATH 等常规位置发现；经环境变量
# PRECESS_PYTHON_PATH 指定 python.exe 所在目录时优先采用（该变量同时前置
# 进运行时 PATH，保证构建与运行使用同一解释器，避免混用）
file(TO_CMAKE_PATH "$ENV{PRECESS_PYTHON_PATH}" _precess_python_dir)
if(_precess_python_dir AND EXISTS "${_precess_python_dir}/python.exe")
    set(Python3_EXECUTABLE "${_precess_python_dir}/python.exe"
        CACHE FILEPATH "构建与运行所用 Python 解释器（经 PRECESS_PYTHON_PATH 指定）" FORCE)
endif()
unset(_precess_python_dir)

# Development.Embed（内嵌宿主需要，见 precess_runtime）：须在本文件内、
# find_package(pybind11) 之前一并请求——pybind11::embed 仅在 Python3::Python
# 目标已存在时才把 Python 链接接口挂入自身，时序颠倒会导致宿主消费者缺库
find_package(Python3 COMPONENTS Interpreter Development.Module Development.Embed QUIET)
if (NOT PRECESS_BUILD_PYTHON)
    return()
endif()
if (NOT Python3_FOUND)
    message(STATUS "未找到 Python3 解释器/开发库，跳过 precess 绑定模块构建")
    return()
endif()

# 导出构建所用解释器目录（标准库根/python313.dll 所在），运行时宿主配置
# 与 app 安装依赖收拢共用（跨目录可见的缓存变量）
get_filename_component(PRECESS_PYTHON_HOME_DIR "${Python3_EXECUTABLE}" DIRECTORY)
set(PRECESS_PYTHON_HOME_DIR "${PRECESS_PYTHON_HOME_DIR}" CACHE INTERNAL
    "构建所用 Python 解释器目录（标准库根）")

# pybind11 查找顺序：依赖包提供（CMAKE_PREFIX_PATH）→ 本机 pip 安装
# （python -m pybind11 --cmakedir 定位）；均未命中时配置失败，请先经
# PreCess-deps.py 安装依赖或 pip install pybind11。
find_package(pybind11 CONFIG QUIET)
if (NOT pybind11_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE _pybind11_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _pybind11_result
    )
    find_package(pybind11 CONFIG QUIET HINTS "${_pybind11_dir}")
endif()
if (NOT pybind11_FOUND)
    message(FATAL_ERROR
        "未找到 pybind11：请先运行 PreCess-deps.py 安装依赖包，或 pip install pybind11"
    )
endif()

pybind11_add_module(precess precess_module.cpp)
target_link_libraries(precess PRIVATE
    Session
)

# 内嵌解释器宿主（静态库，无 Qt）：解释器生命周期、precess 导入与活会话注入、
# 控制台执行。wasm 不构建（Embed 缺失时整个 python 目录已提前跳过）
if(Python3_Development.Embed_FOUND AND NOT EMSCRIPTEN)
    add_library(precess_runtime STATIC)
    target_sources(precess_runtime PRIVATE runtime.cpp)
    target_include_directories(precess_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(precess_runtime
        PUBLIC Session
        PRIVATE pybind11::embed spdlog::spdlog
    )
endif()

if (BUILD_TESTING)
    add_test(NAME precess_python_smoke
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/test/test_precess.py" --plugins "${CMAKE_BINARY_DIR}/plugins")
    # Python 3.8 起扩展模块的依赖 DLL 不再经 PATH 解析，须经脚本侧
    # os.add_dll_directory 注册：DLL 目录清单经 PRECESS_DLL_DIRS 传递
    set_tests_properties(precess_python_smoke PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:precess>"
        ENVIRONMENT_MODIFICATION "$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess>,PREPEND,PATH=path_list_prepend:>;$<LIST:TRANSFORM,$<TARGET_RUNTIME_DLL_DIRS:precess>,PREPEND,PRECESS_DLL_DIRS=path_list_append:>")
    if (TARGET precess_runtime)
        # C++ 嵌入测试：验证宿主初始化、控制台执行与活会话调用
        precess_add_test(TestPrecessRuntime test/test_runtime.cpp)
        target_link_libraries(TestPrecessRuntime PRIVATE precess_runtime)
        target_compile_definitions(TestPrecessRuntime PRIVATE
            PRECESS_PYTHON_HOME="${PRECESS_PYTHON_HOME_DIR}"
            PRECESS_PYTHON_MODULE_DIR="$<TARGET_FILE_DIR:precess>"
            PRECESS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins")
    endif()
endif()
