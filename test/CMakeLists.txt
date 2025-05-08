find_package(Qt6 REQUIRED COMPONENTS Core Quick QmlIntegration)
find_package(GTest REQUIRED)

 # 直接让测试目标也能看到 VTK 头文件
find_package(VTK REQUIRED COMPONENTS
     CommonColor CommonCore GUISupportQtQuick FiltersHybrid FiltersSources
     FiltersExtraction InteractionStyle RenderingContextOpenGL2 RenderingCore
     RenderingFreeType RenderingGL2PSOpenGL2 RenderingOpenGL2 IOGeometry
)

add_executable(
  hello_test
  hello_test.cc
        TestModel.h
        test_ModelQuery.cpp
)

target_include_directories(hello_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..     # 保证可以找到 ModelData.h
        ${VTK_INCLUDE_DIRS}                 # 加上 VTK 的 include
)

target_link_libraries(
  hello_test PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::QmlIntegration
        modelPart
        vtkPart
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hello_test)