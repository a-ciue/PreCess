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

# 1) 生成 App Bundle（macOS 上的 .app 目录结构）
qt_add_executable(${MYNAME} MACOSX_BUNDLE main.cpp)
# 设置编译完成的组件在 qrc:/qt/qml
qt_policy(SET QTP0001 NEW)
qt_add_qml_module(${MYNAME}
        URI fileLoader
        QML_FILES
        Main.qml SelectingBar.qml SideBar.qml ObjectList.qml Selector.qml
        SOURCES
        MyVTKItem.h MyVtkItem.cpp ModelManager.h ModelManager.cpp Selection.h ModelOperator.h ModelOperator.cpp ModelObserver.h ModelQuery.cpp
)

add_library(vtkPart STATIC ModelActor.cpp ModelUtil.cpp Selector.cpp Style.cpp FileHandler.cpp)

add_library(modelPart STATIC
        ModelData.cpp
        # ModelManager.cpp
        FileHandler.cpp
        # ModelQuery.cpp
        command/SplitFaceCommand.cpp
        command/MergeBlocksCommand.cpp
        command/LoadSplineCommand.cpp)

# 让 modelPart 自己也能看到 VTK 和 Qt 的头文件
target_include_directories(modelPart
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}   # 项目根目录
)

# 链接 QtCore、QtQuick（如果需要）和所有 VTK 模块
target_link_libraries(modelPart
        PRIVATE
        Qt${VTK_QT_VERSION}::Core
        Qt${VTK_QT_VERSION}::Quick
)

# VTK 模块自动初始化
vtk_module_autoinit(TARGETS ${MYNAME} vtkPart MODULES ${VTK_LIBRARIES} )

target_link_libraries(vtkPart PRIVATE ${VTK_LIBRARIES})
target_link_libraries(${MYNAME}
        PRIVATE
        vtkPart
        modelPart
        Qt${VTK_QT_VERSION}::Core
        Qt${VTK_QT_VERSION}::Quick
        ${VTK_LIBRARIES}
)

# - Prevent the creation of a console window on Windows.
# - Create an application bundle on macOS.
set_target_properties(${MYNAME} PROPERTIES
#    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

target_compile_definitions(${MYNAME} PRIVATE
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>"
)

include(InstallRequiredSystemLibraries)
# 2) 安装 .app 到指定前缀下
install(TARGETS ${MYNAME}
    BUNDLE DESTINATION .          # 放在 install/ 目录下
    FRAMEWORK DESTINATION Frameworks
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 3) 安装依赖库 (RUNTIME_DEPENDENCIES)
#    - 这里要列举实际用到的 VTK::XXX 模块输出目录，否则无法复制对应的 .dylib
install(
        TARGETS ${MYNAME}
        RUNTIME_DEPENDENCIES
        DIRECTORIES
        $<TARGET_FILE_DIR:VTK::CommonCore>
        # Qt 库所在目录
        $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core>
        $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Quick>
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"  # don't install Windows-provided libs
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"  # don't install Windows-provided libs
        FRAMEWORK DESTINATION Frameworks
        BUNDLE DESTINATION .
)

# 4) 生成部署 QML 的脚本（若需要更多 QML 插件，可在这里处理）
qt_generate_deploy_qml_app_script(
        TARGET ${MYNAME}
        OUTPUT_SCRIPT deploy_script
)
install(SCRIPT ${deploy_script})

# 5) 使用 fixup_bundle 将所有依赖“收拢”进 .app，并修正引用路径（仅在 macOS 下执行）
if(APPLE)
  set(APP_BUNDLE_EXECUTABLE "${CMAKE_INSTALL_PREFIX}/${MYNAME}.app/Contents/MacOS/${MYNAME}")

  install(CODE "
    include(BundleUtilities)
    # 使用 $ENV{DESTDIR} 拼接最终路径
    set(BUNDLE_PATH \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${MYNAME}.app/Contents/MacOS/${MYNAME}\")
    message(STATUS \"fixup_bundle: processing \${BUNDLE_PATH}\")
    fixup_bundle(
      \"\${BUNDLE_PATH}\"
      \"\"
      \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Frameworks;/opt/homebrew/lib;/usr/local/lib\"
    )
  ")
endif()


if(ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()