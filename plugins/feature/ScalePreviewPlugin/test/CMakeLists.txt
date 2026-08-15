precess_add_test(TestScalePreviewPlugin TestScalePreviewPlugin.cpp)
precess_test_link_libraries(TestScalePreviewPlugin ScalePreviewPluginlib Systems FeatureSystem Data Qt6::Core)

if(BUILD_TESTING)
    # 被测插件 dll 的路径注入测试，且测试运行前需先构建插件
    target_compile_definitions(TestScalePreviewPlugin PRIVATE SCALE_PREVIEW_PLUGIN_PATH="$<TARGET_FILE:ScalePreviewPlugin>")
    add_dependencies(TestScalePreviewPlugin ScalePreviewPlugin)
endif()
