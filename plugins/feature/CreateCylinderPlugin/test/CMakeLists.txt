precess_add_test(TestCreateCylinderPlugin TestCreateCylinderPlugin.cpp)
precess_test_link_libraries(TestCreateCylinderPlugin CreateCylinderPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreateCylinderPlugin PRIVATE CREATE_CYLINDER_PLUGIN_PATH="$<TARGET_FILE:CreateCylinderPlugin>")
    add_dependencies(TestCreateCylinderPlugin CreateCylinderPlugin)
endif()
