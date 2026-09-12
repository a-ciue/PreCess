find_package(KDDockWidgets-qt6 REQUIRED)

add_subdirectory(core)
add_subdirectory(model)
add_subdirectory(render)

# 1) 生成 App Bundle（macOS 上的 .app 目录结构）
set(app_resource "")
if(WIN32)
    set(app_resource "${CMAKE_SOURCE_DIR}/resource/PreCess.rc")
endif()
qt_add_executable(${MYNAME} MACOSX_BUNDLE main.cpp ${app_resource})
# 设置编译完成的组件在 qrc:/qt/qml
qt_add_qml_module(${MYNAME}
        URI app
        SOURCES
            TreeModel.h
            TreeModel.cpp
            QLogManager.h
            QLogManager.cpp
        QML_FILES
            Main.qml
            AppToolbar.qml
            AttributeRenderPanel.qml
            CentralRenderArea.qml
            SideBar.qml
            ObjectTree.qml
            Selector.qml
            JavaScriptConsole.qml
            PythonConsole.qml
            PreferencesWindow.qml
            OutputLog.qml
)
target_link_libraries(${MYNAME}
    PRIVATE
    renderQml renderQmlplugin modelQml modelQmlplugin
    VTK::GUISupportQtQuick
    Qt${VTK_QT_VERSION}::Quick
    Qt${VTK_QT_VERSION}::Svg
    KDAB::kddockwidgets
    spdlog::spdlog
    "$<TARGET_NAME_IF_EXISTS:StaticPlugins>"
)

target_compile_definitions(${MYNAME} PRIVATE
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>"
)

# - Prevent the creation of a console window on Windows.
# - Create an application bundle on macOS.
set_target_properties(${MYNAME} PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(APPLE)
    set(app_icon_macos "${CMAKE_SOURCE_DIR}/resource/PreCess.icns")
    target_sources(${MYNAME} PRIVATE ${app_icon_macos})
    set_source_files_properties(${app_icon_macos} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${MYNAME} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE PreCess.icns
        MACOSX_BUNDLE_GUI_IDENTIFIER com.precess.PreCess
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

if(EMSCRIPTEN)
target_sources(${MYNAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/QWasmBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/QWasmBridge.h"
)
# 静态 wasm 构建需要显式导入图片格式插件
qt_import_plugins(${MYNAME} INCLUDE
    Qt6::QSvgPlugin
    Qt6::QSvgIconPlugin
    Qt6::QJpegPlugin
    Qt6::QGifPlugin
    Qt6::QICOPlugin
)
# OCCT 8.0 起 BRepMesh_IncrementalMeshFactory 使用静态初始化器注册，
# 但会被 wasm 链接器优化掉，这里强制链接 TKMesh 静态库解决。
target_link_options(${MYNAME} PRIVATE
    "LINKER:--whole-archive"
    "$<TARGET_FILE:TKMesh>"
    "LINKER:--no-whole-archive"
)
endif()

# 2) 安装 .app 到指定前缀下
include(InstallRequiredSystemLibraries)
install(TARGETS ${MYNAME}
    BUNDLE DESTINATION .          # 放在 install/ 目录下
    FRAMEWORK DESTINATION Frameworks
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)

# 3) 安装依赖库 (RUNTIME_DEPENDENCIES)
#    - 这里要列举实际用到的模块动态库目录，否则无法复制对应的运行时依赖
#    - wasm 全静态链接，无动态库依赖扫描与 QML 部署脚本
if(NOT EMSCRIPTEN)
set(DEPENDENCY_DIRECTORIES
    $<TARGET_FILE_DIR:VTK::CommonCore>
    $<TARGET_FILE_DIR:TKernel>
    $<TARGET_FILE_DIR:Qt${VTK_QT_VERSION}::Core>
    $<TARGET_FILE_DIR:spdlog::spdlog>
    $<TARGET_FILE_DIR:KDAB::kddockwidgets>
)
if(freetype_FOUND)
    list(APPEND DEPENDENCY_DIRECTORIES $<TARGET_FILE_DIR:freetype>)
endif()
# 内嵌 Python 时把构建所用解释器目录（python/ 导出的缓存变量）加入依赖搜索，
# 使 install 能收拢解释器运行库
if(PRECESS_PYTHON_HOME_DIR)
    list(APPEND DEPENDENCY_DIRECTORIES "${PRECESS_PYTHON_HOME_DIR}")
endif()
set(DEPENDENCY_POST_EXCLUDE_REGEXES ".*system32/.*\\.dll")
if(APPLE)
    list(APPEND DEPENDENCY_POST_EXCLUDE_REGEXES
        "/usr/lib/.*"
        "/System/Library/.*"
    )
elseif(UNIX)
    list(APPEND DEPENDENCY_POST_EXCLUDE_REGEXES
        ".*libc\\.so.*"
        ".*libm\\.so.*"
        ".*libdl\\.so.*"
        ".*libpthread\\.so.*"
        ".*librt\\.so.*"
        ".*libnsl\\.so.*"
        ".*libstdc\\+\\+\\.so.*"
        ".*libgcc_s\\.so.*"
        ".*libGL\\.so.*"
        ".*libEGL\\.so.*"
        ".*libOpenGL\\.so.*"
        ".*libGLX\\.so.*"
        ".*libxkbcommon\\.so.*"
        ".*libwayland-.*\\.so.*"
        ".*libX[a-zA-Z0-9]*\\.so.*"
        ".*libfontconfig\\.so.*"
        ".*libglib-2\\.0\\.so.*"
    )
endif()
install(
    TARGETS ${MYNAME}
    RUNTIME_DEPENDENCIES
    DIRECTORIES ${DEPENDENCY_DIRECTORIES}
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" # don't install system provided libs
    POST_EXCLUDE_REGEXES ${DEPENDENCY_POST_EXCLUDE_REGEXES}
    FRAMEWORK DESTINATION $<IF:$<PLATFORM_ID:Darwin>,${MYNAME}.app/Contents/Frameworks,Frameworks>
    LIBRARY DESTINATION $<IF:$<PLATFORM_ID:Darwin>,${MYNAME}.app/Contents/Frameworks,lib>
    RUNTIME DESTINATION $<IF:$<PLATFORM_ID:Darwin>,${MYNAME}.app/Contents/MacOS,bin>
    BUNDLE DESTINATION .
    COMPONENT ${MYNAME}
)

# 4) 生成部署 QML 的脚本（若需要更多 QML 插件，可在这里处理）
qt_generate_deploy_qml_app_script(
    TARGET ${MYNAME}
    OUTPUT_SCRIPT deploy_script
    NO_COMPILER_RUNTIME # 不在此包含运行时库，特别是vc_redist.exe。已通过InstallRequiredSystemLibraries声明安装运行时dll
#    DEPLOY_TOOL_OPTIONS --no-opengl-sw --no-system-dxc-compiler # 可选通过禁用一些功能来减小体积
)
install(
    SCRIPT ${deploy_script}
    COMPONENT ${MYNAME}
)
endif()

# 5) 使用 fixup_bundle 将所有依赖“收拢”进 .app，并修正引用路径（仅在 macOS 下执行）
if(APPLE)
  install(CODE "
    include(BundleUtilities)
    # 使用 $ENV{DESTDIR} 拼接最终路径
    set(BUNDLE_PATH \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${MYNAME}.app/Contents/MacOS/${MYNAME}\")
    # 插件经运行期加载、不在主程序链接依赖内，一并交给 fixup_bundle 解析依赖并改写引用
    file(GLOB plugin_libraries
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${MYNAME}.app/Contents/plugins/*.dylib\")
    message(STATUS \"fixup_bundle: processing \${BUNDLE_PATH}\")
    fixup_bundle(
      \"\${BUNDLE_PATH}\"
      \"\${plugin_libraries}\"
      \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${MYNAME}.app/Contents/Frameworks;\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib;/opt/homebrew/lib;/usr/local/lib\"
    )
  ")
endif()

# 6) 构建 wasm 平台，捆绑字体，生成 HTML 壳，并安装网页侧全部产物
if(EMSCRIPTEN)
set(APPNAME ${MYNAME})
# 与 Qt6WasmMacros 的默认导出名规则保持一致（<target>_entry）
string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" wasm_target_name "${MYNAME}")
set(APPEXPORTNAME "${wasm_target_name}_entry")
string(TIMESTAMP CACHE_ID "%Y%m%d%H%M%S" UTC)
configure_file("${CMAKE_SOURCE_DIR}/resource/pwa/index.html" "${CMAKE_CURRENT_BINARY_DIR}/index.html" @ONLY)
configure_file("${CMAKE_SOURCE_DIR}/resource/pwa/sw.js" "${CMAKE_CURRENT_BINARY_DIR}/sw.js" @ONLY)

# 捆绑字体资源，生成 qrc:/fonts/appfont.bin
set(WASM_FONT_FILE "" CACHE FILEPATH "嵌入资源的字体文件（默认使用思源黑体）")
if(NOT WASM_FONT_FILE)
    if(EXISTS "${WASM_QT_DIR}/../fonts/SourceHanSansCN-Regular.otf")
        set(WASM_FONT_FILE "${WASM_QT_DIR}/../fonts/SourceHanSansCN-Regular.otf")
    endif()
endif()
if(WASM_FONT_FILE)
    message(STATUS "wasm use font: ${WASM_FONT_FILE}")
    set(WASM_FONT_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/appfont.bin")
    file(COPY_FILE "${WASM_FONT_FILE}" "${WASM_FONT_EMBEDDED}")
    set_source_files_properties("${WASM_FONT_EMBEDDED}" PROPERTIES
        QT_RESOURCE_ALIAS "appfont.bin")
    qt_add_resources(${MYNAME} "appfont"
        FILES "${WASM_FONT_EMBEDDED}"
        PREFIX "/fonts"
    )
else()
    message(WARNING "未找到可嵌入的字体，中文将显示为方框，请将 WASM_FONT_FILE 设置为指定的字体路径")
endif()

# 安装其他产物
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qtloader.js"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${MYNAME}.wasm"
    "${CMAKE_CURRENT_BINARY_DIR}/${MYNAME}.data"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
    OPTIONAL
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${MYNAME}.worker.js"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
    OPTIONAL
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../resource/PreCess.ico"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)
install(FILES
    "${CMAKE_SOURCE_DIR}/resource/pwa/manifest.json"
    "${CMAKE_CURRENT_BINARY_DIR}/sw.js"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${MYNAME}
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resource/pwa/icons/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/icons"
    COMPONENT ${MYNAME}
)
endif()
