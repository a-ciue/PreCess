find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest REQUIRED)

add_executable(
  hello_test
  hello_test.cc
        TestModel.h
        test_ModelQuery.cpp
)

target_include_directories(hello_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..     # 保证可以找到 Model.h
)

target_link_libraries(
  hello_test PRIVATE
        Qt6::Core
        modelPart
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hello_test)