# CGAL 集成基础库（GPLv3 依赖，仅允许 plugins/ 内使用）
# CMake≥3.30 已移除 FindBoost 模块，显式声明走 CONFIG 模式以消除 dev 警告
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
# 暂缓 GMP/MPFR：CGAL_DISABLE_GMP 跳过查找，精确数类型回退内置 MP_Float（正确性不变、性能略降），
# 后续引入布尔等精确运算密集算法时再补 GMP
set(CGAL_DISABLE_GMP ON)
find_package(CGAL REQUIRED)

# CGAL 仅需 Boost 头文件；boost 源树不带 config 包，按路径搜索（自动检索 CMAKE_PREFIX_PATH）
find_path(BOOST_INCLUDE_DIR boost/version.hpp
    PATH_SUFFIXES boost_1_87_0 boost
    REQUIRED)

add_library(CgalSupport STATIC
    CgalMeshAdapter.cpp
)
# 关键：CGAL_DISABLE_GMP 必须同时作为【编译宏】传播——CGAL/CORE/BigInt.h 在未定义该宏时
# 默认启用 GMP 后端（boost mpz_int，需 gmp.h），会带出大片模板错误；
# 定义后回退 boost cpp_int（纯头文件）。CMake 变量 CGAL_DISABLE_GMP 只控制 CGALConfig 的查找逻辑
target_compile_definitions(CgalSupport PUBLIC CGAL_DISABLE_GMP)
target_include_directories(CgalSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${BOOST_INCLUDE_DIR})
target_link_libraries(CgalSupport PUBLIC Core Data CGAL::CGAL)

# MSVC 默认不启用 C++ 异常（本项目全局未开 /EHsc），会导致 Boost 判定 BOOST_NO_EXCEPTIONS、
# boost::throw_exception 模板不实例化而产生链接错误；同时 CGAL 头文件大量 C4530 警告。
# 对 CGAL 消费目标统一启用（PUBLIC 传播到链接本库的插件与测试）
if(MSVC)
    target_compile_options(CgalSupport PUBLIC /EHsc)
endif()

add_subdirectory(test)
