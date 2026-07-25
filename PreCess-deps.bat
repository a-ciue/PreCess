@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

if "%~1"=="" (
    echo 请提供文件夹路径
    echo.
    echo usage: PreCess-deps "dependency_dir" [--release] [--no-relinfo] [--qt Qt_path]
    echo.
    echo    --release     同时构建 Release 配置（默认仅构建 RelWithDebInfo + Debug）
    echo    --no-relinfo  跳过 RelWithDebInfo 配置的构建
    echo    --qt path     指定现有 Qt 安装路径（如 C:/Qt/6.8.3/msvc2022_64），跳过 Qt 源码编译
    echo                 该路径会与依赖目录并列加入 CMAKE_PREFIX_PATH
    echo.
    echo 请务必在vc++构建环境中，如"x64 Native Tools Command Prompt for VS 2022"，进行依赖构建
    echo 脚本支持在初始化时自动识别已经开启的系统代理
    pause
    exit /b 1
)

REM 直接转换为绝对路径
set "depsPath=%~f1"

REM 解析可选参数 (支持灵活顺序)
set "buildRelease=0"
set "buildRelInfo=1"
shift
:parseArgs
if "%~1"=="" goto :parsed
if "%~1"=="--release" set "buildRelease=1"
if "%~1"=="--no-relinfo" set "buildRelInfo=0"
if "%~1"=="--qt" (
    if "%~2"=="" (
        echo 错误: --qt 需要指定路径
        exit /b 1
    )
    set "qtPath=%~f2"
    shift
)
shift
goto :parseArgs
:parsed

echo 待配置依赖路径：%depsPath%

if not exist "%depsPath%" (
    mkdir "%depsPath%" || (
        echo 错误：无法创建路径 "%depsPath%"
        exit /b 1
    )
    echo 已创建路径 "%depsPath%"
)

pushd "%depsPath%"

echo 正在查询系统代理设置...
echo.
REM 查询是否启用代理
for /f "tokens=3" %%i in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable 2^>nul') do (
    set "ProxyEnable=%%i"
)
if "!ProxyEnable!"=="0x1" (
    echo [状态] 系统代理已启用
    echo.

    REM 获取代理服务器地址
    for /f "tokens=2,*" %%i in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyServer 2^>nul') do (
        set "ProxyServer=%%j"
    )

    if defined ProxyServer (
        echo [代理地址] !ProxyServer!
        set "HTTP_PROXY=!ProxyServer!"
        set "HTTPS_PROXY=!ProxyServer!"
    ) else (
        echo [代理地址] 未设置具体地址
    )

)

REM 你的操作在这里
mkdir _source
pushd _source
set "sourcePath=%depsPath%/_source"

REM Clone Repos
REM -single-branch 防止减少clone的下载量
if not defined qtPath git clone --single-branch --depth 1 --branch v6.8.3 https://code.qt.io/qt/qt5.git
git clone --single-branch --depth 1 --branch v2.4.0 https://github.com/KDAB/KDDockWidgets.git
git clone --single-branch --depth 1 --branch v9.6.2 https://gitlab.kitware.com/vtk/vtk.git
git clone --single-branch --depth 1 --branch VER-2-14-1 https://gitlab.freedesktop.org/freetype/freetype.git
git clone --single-branch --depth 1 --branch V8_0_0 https://github.com/Open-Cascade-SAS/OCCT.git
git clone --single-branch --depth 1 --branch v1.16.0 https://github.com/gabime/spdlog.git
git clone --single-branch --depth 1 --branch v3.11.0 https://github.com/catchorg/Catch2.git
git clone --single-branch --depth 1 --branch v7.80 https://github.com/LoicMarechal/libMeshb.git
git clone --single-branch --depth 1 --branch v1.6.0 https://github.com/TetGen/TetGen.git tetgen
git clone --single-branch --branch master https://gitlab.onelab.info/gmsh/gmsh.git gmsh-occ8
pushd gmsh-occ8
git checkout --detach 86596d7902a1b00e23641ac5c904b7c1f880ce9f
popd
curl -L -o OCCT/3rdparty-vc14-64-temp.zip --connect-timeout 30 https://github.com/Open-Cascade-SAS/OCCT/releases/download/V8_0_0/3rdparty-vc14-64.zip

if not defined qtPath (
    REM Clone and build Qt 6.8.3 source code
    pushd "%sourcePath%/qt5"
    mkdir build
    pushd build
    set "qtConfig=-debug-and-release"
    if "!buildRelease!"=="0" if "!buildRelInfo!"=="0" set "qtConfig=-debug"
    call "../configure" -init-submodules -submodules qtdeclarative !qtConfig! -prefix "%depsPath%\Qt6.8.3" CMAKE_INSTALL_MESSAGE=LAZY
    cmake --build . --parallel --target install
    popd
    popd
) else (
    echo 使用外部 Qt: !qtPath!
)

set "prefixPath=%depsPath%"
if defined qtPath set "prefixPath=%depsPath%;!qtPath!"

REM Clone and build spdlog 1.16.0
pushd "%sourcePath%/spdlog"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i -DSPDLOG_BUILD_SHARED=1 "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\spdlog1.16.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build KDDockWidgets 2.4.0
pushd "%sourcePath%/KDDockWidgets"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_PREFIX_PATH:PATH=!prefixPath!" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\KDDockWidgets-qt6-2.4.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build VTK 9.6.2
pushd "%sourcePath%/vtk"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_PREFIX_PATH:PATH=!prefixPath!" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\VTK9.6.2" -DVTK_SMP_IMPLEMENTATION_TYPE:STRING="STDThread" -DVTK_GROUP_ENABLE_Qt:STRING="WANT" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build freetype 2.14.1
pushd "%sourcePath%/freetype"
cmake -S . -B ./build "-GNinja Multi-Config" "-DCMAKE_PREFIX_PATH:PATH=!prefixPath!" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\freetype2.14.1" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config Release
cmake --build . --target install --config Debug

REM Clone and build OpenCASCADE 8.0.0
pushd "%sourcePath%/OCCT"
tar -xf ./3rdparty-vc14-64-temp.zip -C .
tar -xf ./3rdparty-vc14-64.zip -C .
cmake -S . -B ./build "-GNinja Multi-Config" "-DINSTALL_DIR:PATH=%depsPath%\OpenCASCADE8.0.0" "-D3RDPARTY_DIR:PATH=%cd%/3rdparty-vc14-64" "-D3RDPARTY_FREETYPE_DIR:PATH=%depsPath%\freetype2.14.1" -DUSE_VTK:BOOL=1 "-D3RDPARTY_VTK_DIR:PATH=%depsPath%\VTK9.6.2" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build Catch2 3.11.0
pushd "%sourcePath%/Catch2"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\Catch2-3.11.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build libMeshb 7.80
pushd "%sourcePath%/libMeshb"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\libMeshb7.80" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build tetgen 1.6.0
pushd "%sourcePath%/tetgen"
REM tetgen upstream has no CMakeLists.txt, write one with install rules
echo cmake_minimum_required(VERSION 3.5^)> CMakeLists.txt
echo project(tetgen CXX)>> CMakeLists.txt
echo add_library(tet STATIC tetgen.cxx predicates.cxx^)>> CMakeLists.txt
echo target_compile_definitions(tet PUBLIC TETLIBRARY^)>> CMakeLists.txt
echo target_include_directories(tet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}^)>> CMakeLists.txt
echo install(TARGETS tet ARCHIVE DESTINATION lib^)>> CMakeLists.txt
echo install(FILES tetgen.h DESTINATION include^)>> CMakeLists.txt
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i -DCMAKE_DEBUG_POSTFIX=d "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\tetgen1.6.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build Gmsh OCC8
set "CASROOT=%depsPath%\OpenCASCADE8.0.0"
set "casRootCmake=%CASROOT:\=/%"
set "occLibs="

REM OCC 8 的导入库：Debug 在 libd，Release 在 lib，RelWithDebInfo 在 libi。
for %%L in (TKDESTEP TKDEIGES TKXSBase TKOffset TKFeat TKFillet TKBool TKMesh TKHLR TKBO TKPrim TKShHealing TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel) do (
    if defined occLibs set "occLibs=!occLibs!;"
    set "occLibs=!occLibs!$<IF:$<CONFIG:Debug>,!casRootCmake!/win64/vc14/libd/%%L.lib,$<IF:$<CONFIG:RelWithDebInfo>,!casRootCmake!/win64/vc14/libi/%%L.lib,!casRootCmake!/win64/vc14/lib/%%L.lib>>"
)

pushd "%sourcePath%\gmsh-occ8"

cmake -S . -B ./build ^
    "-GNinja Multi-Config" "-DCMAKE_CONFIGURATION_TYPES:STRING=Debug;Release;RelWithDebInfo" "-DCMAKE_DEBUG_POSTFIX:STRING=d" "-DCMAKE_RELWITHDEBINFO_POSTFIX:STRING=i" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\gmsh-occ8" -DENABLE_OCC:BOOL=ON "-DOCC_LIBS:STRING=!occLibs!" -DENABLE_OPENMP:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DENABLE_BUILD_DYNAMIC:BOOL=OFF -DENABLE_BUILD_LIB:BOOL=OFF -DENABLE_BUILD_SHARED:BOOL=ON -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build

cmake --build . --target install --config Debug
if "!buildRelInfo!"=="1" cmake --build . --target install --config RelWithDebInfo
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM 把 gmshTargets.cmake 中的 OpenCASCADE 绝对路径改为相对路径
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$path = '%depsPath%\gmsh-occ8\share\gmsh\gmshTargets.cmake';" ^
    "$oldForward = '%depsPath:\=/%/OpenCASCADE8.0.0';" ^
    "$oldBackward = '%depsPath%\OpenCASCADE8.0.0';" ^
    "$newPath = '${_IMPORT_PREFIX}/../OpenCASCADE8.0.0';" ^
    "$content = [System.IO.File]::ReadAllText($path);" ^
    "$updated = $content.Replace($oldForward, $newPath).Replace($oldBackward, $newPath);" ^
    "if ($updated -eq $content) {" ^
    "    if ($content.Contains($newPath)) {" ^
    "        Write-Host 'OpenCASCADE 路径已经是相对路径。';" ^
    "        exit 0;" ^
    "    }" ^
    "    throw '没有在 gmshTargets.cmake 中找到指定的 OpenCASCADE 绝对路径。';" ^
    "}" ^
    "[System.IO.File]::WriteAllText($path, $updated, [System.Text.UTF8Encoding]::new($false));" ^
    "Write-Host 'OpenCASCADE 路径修改成功。';"

if errorlevel 1 (
    echo.
    echo 修改失败。
    pause
    exit /b 1
)

popd

echo 处理完成！

endlocal
