add_subdirectory(core)
add_subdirectory(model)
add_subdirectory(render)

# 1) 生成 App Bundle（macOS 上的 .app 目录结构）
set(app_icon_resource_windows "${CMAKE_SOURCE_DIR}/resource/PreCess.rc")
qt_add_executable(${MYNAME} MACOSX_BUNDLE main.cpp ${app_icon_resource_windows})
# 设置编译完成的组件在 qrc:/qt/qml
qt_add_qml_module(${MYNAME}
        URI app
        QML_FILES
        Main.qml SelectingBar.qml SideBar.qml ObjectList.qml ObjectListModel.qml Selector.qml CommandMenu.qml
)
target_link_libraries(${MYNAME}
    PRIVATE
    renderQml renderQmlplugin modelQml modelQmlplugin
    VTK::GUISupportQtQuick
    Qt${VTK_QT_VERSION}::Quick
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
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)

# 3) 安装依赖库 (RUNTIME_DEPENDENCIES)
#    - 这里要列举实际用到的 VTK::XXX 模块输出目录，否则无法复制对应的 .dylib
install(
        TARGETS ${MYNAME}
        RUNTIME_DEPENDENCIES
        DIRECTORIES
            $<TARGET_FILE_DIR:VTK::CommonCore>
            $<TARGET_FILE_DIR:TKernel>
            $<TARGET_FILE_DIR:freetype>
             # Qt 库所在目录
            $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core>
            $<TARGET_FILE_DIR:spdlog::spdlog>
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"  # don't install Windows-provided libs
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"  # don't install Windows-provided libs
        FRAMEWORK DESTINATION Frameworks
        BUNDLE DESTINATION .
        COMPONENT ${MYNAME}
)

# 4) 生成部署 QML 的脚本（若需要更多 QML 插件，可在这里处理）
qt_generate_deploy_qml_app_script(
        TARGET ${MYNAME}
        OUTPUT_SCRIPT deploy_script
)
install(
    SCRIPT ${deploy_script}
    COMPONENT ${MYNAME}
)

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
