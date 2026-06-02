@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

if "%~1"=="" (
    echo 请提供文件夹路径
    echo.
    echo usage: PreCess-deps "dependency_dir" [--release]
    echo.
    echo    --release  同时构建 Release 配置（默认仅构建 RelWithDebInfo + Debug）
    echo.
    echo 请务必在vc++构建环境中，如"x64 Native Tools Command Prompt for VS 2022"，进行依赖构建
    echo 脚本支持在初始化时自动识别已经开启的系统代理
    pause
    exit /b 1
)

REM 直接转换为绝对路径
set "depsPath=%~f1"

REM 解析可选参数
set "buildRelease=0"
if "%~2"=="--release" set "buildRelease=1"

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
git clone --single-branch --depth 1 --branch v6.8.3 https://code.qt.io/qt/qt5.git
git clone --single-branch --depth 1 --branch v2.4.0 https://github.com/KDAB/KDDockWidgets.git
git clone --single-branch --depth 1 --branch v9.6.2 https://gitlab.kitware.com/vtk/vtk.git
git clone --single-branch --depth 1 --branch VER-2-14-1 https://gitlab.freedesktop.org/freetype/freetype.git
git clone --single-branch --depth 1 --branch V8_0_0 https://github.com/Open-Cascade-SAS/OCCT.git
git clone --single-branch --depth 1 --branch v1.16.0 https://github.com/gabime/spdlog.git
git clone --single-branch --depth 1 --branch v3.11.0 https://github.com/catchorg/Catch2.git
git clone --single-branch --depth 1 --branch v7.80 https://github.com/LoicMarechal/libMeshb.git

REM Clone and build Qt 6.8.3 source code
pushd "%sourcePath%/qt5"
mkdir build
pushd build
call "../configure" -init-submodules -submodules qtdeclarative -debug-and-release -prefix "%depsPath%\Qt6.8.3" CMAKE_INSTALL_MESSAGE=LAZY
cmake --build . --parallel --target install

REM Clone and build KDDockWidgets 2.4.0
pushd "%sourcePath%/KDDockWidgets"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_PREFIX_PATH:PATH=%depsPath%" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\KDDockWidgets-qt6-2.4.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build VTK 9.6.2
pushd "%sourcePath%/vtk"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_PREFIX_PATH:PATH=%depsPath%" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\VTK9.6.2" -DVTK_SMP_IMPLEMENTATION_TYPE:STRING="STDThread" -DVTK_GROUP_ENABLE_Qt:STRING="WANT" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build freetype 2.14.1
pushd "%sourcePath%/freetype"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_PREFIX_PATH:PATH=%depsPath%" "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\freetype2.14.1" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config Release
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug

REM Clone and build OpenCASCADE 8.0.0
pushd "%sourcePath%/OCCT"
curl -L -o 3rdparty-vc14-64-temp.zip --connect-timeout 30 https://github.com/Open-Cascade-SAS/OCCT/releases/download/V8_0_0/3rdparty-vc14-64.zip
tar -xf ./3rdparty-vc14-64-temp.zip -C .
tar -xf ./3rdparty-vc14-64.zip -C .
cmake -S . -B ./build "-GNinja Multi-Config" "-DINSTALL_DIR:PATH=%depsPath%\OpenCASCADE8.0.0" "-D3RDPARTY_DIR:PATH=%cd%/3rdparty-vc14-64" "-D3RDPARTY_FREETYPE_DIR:PATH=%depsPath%\freetype2.14.1" -DUSE_VTK:BOOL=1 "-D3RDPARTY_VTK_DIR:PATH=%depsPath%\VTK9.6.2" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build spdlog 1.16.0
pushd "%sourcePath%/spdlog"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\spdlog1.16.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build Catch2 3.11.0
pushd "%sourcePath%/Catch2"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\Catch2-3.11.0" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

REM Clone and build libMeshb 7.80
pushd "%sourcePath%/libMeshb"
cmake -S . -B ./build "-GNinja Multi-Config" -DCMAKE_RELWITHDEBINFO_POSTFIX=i "-DCMAKE_INSTALL_PREFIX:PATH=%depsPath%\libMeshb7.80" -DCMAKE_INSTALL_MESSAGE=LAZY
pushd build
cmake --build . --target install --config RelWithDebInfo
cmake --build . --target install --config Debug
if "!buildRelease!"=="1" cmake --build . --target install --config Release

echo 处理完成！

endlocal