# 真实子进程用于覆盖静态单例在进程退出时的清理行为。
qt_add_executable(TempFileProcess TempFileProcess.cpp)
target_link_libraries(TempFileProcess PRIVATE Core)

precess_add_test(TestTempFile TestTempFile.cpp)
precess_test_link_libraries(TestTempFile Core Qt6::Core)
target_compile_definitions(TestTempFile PRIVATE
    TEMPFILE_PROCESS_PATH="$<TARGET_FILE:TempFileProcess>")
add_dependencies(TestTempFile TempFileProcess)
