precess_add_test(TestCreateSpherePlugin TestCreateSpherePlugin.cpp)
precess_test_link_libraries(TestCreateSpherePlugin CreateSpherePluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestCreateSpherePlugin PRIVATE CREATE_SPHERE_PLUGIN_PATH="$<TARGET_FILE:CreateSpherePlugin>")
    add_dependencies(TestCreateSpherePlugin CreateSpherePlugin)
endif()
