cmake_minimum_required(VERSION 3.14)

set(MYNAME triangulation)

project(${MYNAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


option(ENABLE_TEST "是否启用测试")

find_package(spdlog REQUIRED)

find_package(OpenCASCADE REQUIRED)

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
        RenderingAnnotation
)

find_package("Qt${VTK_QT_VERSION}" REQUIRED COMPONENTS Gui Quick)
message(STATUS "QT_VERSION: Qt${QT_VERSION_MAJOR}")
qt_standard_project_setup()

# 1) 生成 App Bundle（macOS 上的 .app 目录结构）
qt_add_executable(${MYNAME} MACOSX_BUNDLE main.cpp)
# 设置编译完成的组件在 qrc:/qt/qml
qt_policy(SET QTP0001 NEW)
qt_add_qml_module(${MYNAME}
        URI fileLoader
        QML_FILES
        Main.qml SelectingBar.qml SideBar.qml ObjectList.qml Selector.qml CommandMenu.qml
        SOURCES
        MyVTKItem.h MyVtkItem.cpp QSelection.h 
)

qt_add_library(modelQml STATIC)
target_link_libraries(modelQml 
    PRIVATE
    ${OpenCASCADE_LIBRARIES}
)                    
qt_add_qml_module(modelQml 
    URI model
    SOURCES
        ModelManager.h ModelManager.cpp ModelObserver.h ModelQuery.cpp QModelManager.h QModelManager.cpp
)
add_library(vtkPart STATIC MeshActor.cpp Selector.cpp Style.cpp SelectManager.cpp SplineActor.cpp MeshActorManager.cpp SplineActorManager.cpp
        MeshData.h
        SplineData.h
        CTMeshModel.cpp
        MeshData.cpp)


add_library(modelPart STATIC
        ModelData.cpp
        ModelOperator.cpp 
        # ModelManager.cpp 
        ModelUtil.cpp
        FileHandler.cpp
        MeshData.h
        SplineData.h
        MeshData.cpp
        ModelImporter.cpp
        # ModelQuery.cpp
        # commands/SplitFaceCommand.cpp
        # command/MergeBlocksCommand.cpp
        # command/LoadSplineCommand.cpp
)
add_subdirectory(systems)

# 让 modelPart 自己也能看到 VTK 和 Qt 的头文件
target_include_directories(modelPart
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}   # 项目根目录
)
target_link_libraries(modelPart
        PRIVATE
        ${OpenCASCADE_LIBRARIES}      # OCCT 的库列表
        Systems
)
target_link_libraries(vtkPart
        PRIVATE
        ${OpenCASCADE_LIBRARIES}
)
target_link_libraries(vtkPart
    PRIVATE
    TKernel TKMath TKGeomBase TKGeomAlgo TKTopAlgo TKPrim TKShHealing TKMesh TKMeshVS TKService TKIVtk TKDESTEP
    ${VTK_LIBRARIES}
)
target_link_libraries(${MYNAME}
    PRIVATE
    TKernel TKMath TKGeomBase TKGeomAlgo TKTopAlgo TKPrim TKShHealing TKMesh TKMeshVS TKService TKIVtk TKDESTEP
    ${VTK_LIBRARIES}
)

# 链接 QtCore、QtQuick（如果需要）和所有 VTK 模块
target_link_libraries(modelPart
        PRIVATE
        Qt${VTK_QT_VERSION}::Gui
        Qt${VTK_QT_VERSION}::Quick
        Qt${VTK_QT_VERSION}::Core
)


add_subdirectory(commands)
target_link_libraries(modelQml PRIVATE modelPart commandsplugin)

# VTK 模块自动初始化
vtk_module_autoinit(TARGETS ${MYNAME} vtkPart MODULES ${VTK_LIBRARIES} )

target_link_libraries(vtkPart PRIVATE ${VTK_LIBRARIES})
target_link_libraries(${MYNAME}
        PRIVATE
        vtkPart
        modelQmlplugin
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

# 2) 安装 .app 到指定前缀下
include(InstallRequiredSystemLibraries)
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
            $<TARGET_FILE_DIR:TKernel>
            ${FREETYPE_DIR}/bin # FreeType 库所在目录
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