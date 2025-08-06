qt_add_qml_module(coreQml 
    URI core 
	STATIC 
	SOURCES QSelection.h)
target_include_directories(coreQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)
target_link_libraries(coreQml PUBLIC Core)
target_link_libraries(coreQml PRIVATE Qt${VTK_QT_VERSION}::Qml)