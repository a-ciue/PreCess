precess_add_test(TestSessionQuery TestSessionQuery.cpp)
precess_test_link_libraries(TestSessionQuery Session TKPrim TKBRep TKTopAlgo)

precess_add_test(TestSession TestSession.cpp)
precess_test_link_libraries(TestSession Session TKPrim TKBRep TKTopAlgo)

if (BUILD_TESTING)
    precess_add_test(TestSessionPlugins TestSessionPlugins.cpp)
    precess_test_link_libraries(TestSessionPlugins Session TKPrim TKBRep TKTopAlgo Qt6::Core)
    target_compile_definitions(TestSessionPlugins PRIVATE
        PRECESS_PLUGIN_DIR="${CMAKE_BINARY_DIR}/plugins")
endif()
