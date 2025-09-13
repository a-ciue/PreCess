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
#        TestModel.h
#        test_ModelQuery.cpp
)

target_link_libraries(
  hello_test PRIVATE
  modelQml
        Qt6::Core
        Qt6::Qml
        modelPart
        vtkPart
        modelQml
        Core
  GTest::gtest_main
)
set_target_properties(hello_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test_bin
)
add_custom_command(TARGET hello_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:hello_test> $<TARGET_RUNTIME_DLLS:hello_test>
  COMMAND_EXPAND_LISTS
)

include(GoogleTest)
gtest_discover_tests(hello_test
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test_bin
)