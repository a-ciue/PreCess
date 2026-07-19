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
target_include_directories(CgalSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${BOOST_INCLUDE_DIR})
target_link_libraries(CgalSupport PUBLIC Core Data CGAL::CGAL)

add_subdirectory(test)
