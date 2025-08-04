qt_add_library(modelQml STATIC)
target_link_libraries(modelQml 
    PRIVATE
    Data
    ${OpenCASCADE_LIBRARIES}
)
target_link_libraries(modelQml 
    PUBLIC Core Systems
)
qt_add_qml_module(modelQml 
    URI model
    SOURCES
        QModelObserver.h QModelQuery.cpp QModelManager.h QModelManager.cpp
)
target_include_directories(modelQml PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 头文件路径
)