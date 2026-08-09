precess_add_test(TestTetGenLibHandler TestTetGenLibHandler.cpp)
precess_test_link_libraries(TestTetGenLibHandler
    Data
    Systems
    Core
    TetGenLibPluginlib
)
