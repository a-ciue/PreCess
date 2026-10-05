# 聚合目标先于模块子目录创建（空创建 + 后补对象源，python/precess_runtime 同款模式）：
# 模块测试的 precess_test_link_libraries 兜底判断（if(TARGET PreCessBase)）发生在
# 各子目录处理期间，目标须彼时已存在；对象成员在子目录之后经 target_sources 追加，
# $<TARGET_OBJECTS:> 为生成期求值，届时模块目标均已创建。
# 模块目标为 OBJECT 库（模块间 target_link_libraries 依赖图照旧由 CMake 强制，
# AGENTS.md §2 的依赖方向约束不失守），聚合后仍是单个 .lib 供插件与宿主链接；
# SDK 只导出本目标（PreCess::Base），旧目标名（modelPart）经 ALIAS 保持兼容。
add_library(PreCessBase STATIC)
set_target_properties(PreCessBase PROPERTIES
    EXPORT_NAME Base
    # 双配置 SDK 同前缀安装：Debug 产物带 d 后缀（PreCessBased.lib）与 Release 共存、
    # 互不覆盖（多配置生成器单树分 --config 两次安装，见 docs/plugin-sdk-design.md §13#11）
    DEBUG_POSTFIX "d"
)
add_library(PreCess::Base ALIAS PreCessBase)
add_library(modelPart ALIAS PreCessBase)

add_subdirectory(data)
add_subdirectory(systems)
add_subdirectory(ops)
add_subdirectory(session)

target_sources(PreCessBase PRIVATE
    $<TARGET_OBJECTS:Core>
    $<TARGET_OBJECTS:Data>
    $<TARGET_OBJECTS:DataOps>
    $<TARGET_OBJECTS:IOSystem>
    $<TARGET_OBJECTS:Job>
    $<TARGET_OBJECTS:AlgorithmSystem>
    $<TARGET_OBJECTS:EditSystem>
    $<TARGET_OBJECTS:FeatureSystem>
    $<TARGET_OBJECTS:Systems>
    $<TARGET_OBJECTS:Session>
)

# 用法需求聚合点：模块 OBJECT 库的链接依赖不再随链接传递，须在此显式收口，
# 保持与归并前一致的编译/链接语义（均为 find_package 导入目标，可安全随包导出）
# include 目录（BUILD/INSTALL 双布局）：模块清单是头文件布局的唯一事实源，
# 与 PreCessPackage.cmake 的头文件安装（按 core/、model/ 根目录镜像）保持一致。
# - BUILD_INTERFACE 隐藏源码树绝对路径（install(EXPORT) 不允许其进入导出文件）
# - INSTALL_INTERFACE 用安装前缀相对路径（可重定位）
set(_precess_module_dirs
    core
    model
    model/data
    model/ops
    model/systems
    model/systems/core
    model/systems/io
    model/systems/job
    model/systems/algo
    model/systems/edit
    model/systems/feature
    model/session
)
foreach(_dir IN LISTS _precess_module_dirs)
    target_include_directories(PreCessBase PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${_dir}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/precess/${_dir}>
    )
endforeach()
target_link_libraries(PreCessBase PUBLIC
    spdlog::spdlog
    Qt6::Core
    VTK::CommonCore VTK::CommonDataModel
    $<COMPILE_ONLY:TKernel>
    TKernel TKTopAlgo TKBRep TKBool TKPrim TKDESTEP
    $<$<BOOL:${MINGW}>:ws2_32>
)
# std::execution 并行策略后端（原 DataOps 的链接需求，聚合后收口于此）：
# GCC 的 pstl 后端为 TBB 须显式链接；AppleClang 须显式链接 libc++experimental.a；
# MSVC 使用自带的 Windows 线程池后端，无需引入
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(PreCessBase PUBLIC TBB::tbb)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_link_libraries(PreCessBase PUBLIC c++experimental)
endif()
