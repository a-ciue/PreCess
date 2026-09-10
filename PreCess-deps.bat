@echo off
setlocal EnableExtensions
chcp 65001 >nul
set "SCRIPT_DIR=%~dp0"
set "PYTHONUTF8=1"

set "SYSTEM_PROXY_ENABLE="
set "SYSTEM_PROXY_SERVER="
for /f "tokens=3" %%i in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable 2^>nul') do (
    set "SYSTEM_PROXY_ENABLE=%%i"
)
if "%SYSTEM_PROXY_ENABLE%"=="0x1" (
    for /f "tokens=2,*" %%i in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyServer 2^>nul') do (
        set "SYSTEM_PROXY_SERVER=%%j"
    )
)
if defined SYSTEM_PROXY_SERVER (
    echo [代理] 使用 Windows 系统代理 %SYSTEM_PROXY_SERVER%
    set "HTTP_PROXY=%SYSTEM_PROXY_SERVER%"
    set "HTTPS_PROXY=%SYSTEM_PROXY_SERVER%"
) else (
    echo [代理] 未启用 Windows 系统代理
)

where py >nul 2>nul
if %errorlevel% equ 0 (
    py -3 "%SCRIPT_DIR%PreCess-deps.py" %*
) else (
    python "%SCRIPT_DIR%PreCess-deps.py" %*
)
set "DEPS_EXIT_CODE=%errorlevel%"
endlocal & exit /b %DEPS_EXIT_CODE%
