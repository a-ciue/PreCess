precess_add_test(TestCreateLineByCoordinatesPlugin TestCreateLineByCoordinatesPlugin.cpp)
precess_test_link_libraries(TestCreateLineByCoordinatesPlugin CreateLineByCoordinatesPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreateLineByCoordinatesPlugin PRIVATE CREATE_LINE_BY_COORDINATES_PLUGIN_PATH="$<TARGET_FILE:CreateLineByCoordinatesPlugin>")
    add_dependencies(TestCreateLineByCoordinatesPlugin CreateLineByCoordinatesPlugin)
endif()
