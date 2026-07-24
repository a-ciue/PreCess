precess_add_test(TestMeshQualityPlugin TestMeshQualityPlugin.cpp)
precess_test_link_libraries(
    TestMeshQualityPlugin
    MeshQualityPluginlib
    FeatureSystem
    Data
)
