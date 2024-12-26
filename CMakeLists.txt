cmake_minimum_required(VERSION 3.14)

set(MYNAME triangulation)

project(${MYNAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TEST "是否启用测试")

find_package(VTK REQUIRED COMPONENTS
    CommonColor
    CommonCore
    GUISupportQtQuick
    FiltersHybrid
    FiltersSources
    FiltersExtraction
    InteractionStyle
    RenderingContextOpenGL2
    RenderingCore
    RenderingFreeType
    RenderingGL2PSOpenGL2
    RenderingOpenGL2
    IOGeometry
)

find_package("Qt${VTK_QT_VERSION}" REQUIRED COMPONENTS Core Quick)
message(STATUS "QT_VERSION: Qt${QT_VERSION_MAJOR}")
qt_standard_project_setup()

qt_add_executable(${MYNAME} main.cpp)
# 设置编译完成的组件在 qrc:/qt/qml
qt_policy(SET QTP0001 NEW)
qt_add_qml_module(${MYNAME}
    URI fileLoader
    QML_FILES
        Main.qml
    SOURCES
        presenter.h MyVTKItem.h MyVtkItem.cpp ModelManager.h ModelManager.cpp
)

add_library(vtkPart STATIC Model.cpp ModelActor.cpp ModelUtil.cpp Selector.cpp Style.cpp)

vtk_module_autoinit(TARGETS ${MYNAME} vtkPart MODULES ${VTK_LIBRARIES} )

target_link_libraries(vtkPart PRIVATE ${VTK_LIBRARIES})
target_link_libraries(${MYNAME} PRIVATE vtkPart)
target_link_libraries(${MYNAME} PRIVATE Qt${VTK_QT_VERSION}::Core Qt${VTK_QT_VERSION}::Quick)
target_link_libraries(${MYNAME} PRIVATE ${VTK_LIBRARIES})
#if (VTK_QT_VERSION EQUAL 6)
#  qt_import_qml_plugins(${MYNAME})
#  qt_finalize_executable(${MYNAME})
#endif ()

# - Prevent the creation of a console window on Windows.
# - Create an application bundle on macOS.
#set_target_properties(${MYNAME} PROPERTIES
#    WIN32_EXECUTABLE ON
#    MACOSX_BUNDLE ON
#)

target_compile_definitions(${MYNAME} PRIVATE
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>"
)

include(InstallRequiredSystemLibraries)
install(TARGETS ${MYNAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    TARGETS ${MYNAME}
    RUNTIME_DEPENDENCIES
        DIRECTORIES $<TARGET_FILE_DIR:VTK::CommonCore>;$<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core>
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"  # don't install Windows-provided libs
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"  # don't install Windows-provided libs
)

qt_generate_deploy_qml_app_script(
    TARGET ${MYNAME}
    OUTPUT_SCRIPT deploy_script
)
install(SCRIPT ${deploy_script})

if(ENABLE_TEST)
add_subdirectory(test)
endif()
