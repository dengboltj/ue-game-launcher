@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

echo ========================================
echo   Launcher 一键编译
echo ========================================
echo.

set "VCVARS="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    goto :found_vs
  )
)
echo [错误] 未找到 VS2022 x64 编译环境（需要安装“使用 C++ 的桌面开发”）。
pause
exit /b 1

:found_vs
if not exist "logo.ico" if exist "north_logo_1.ico" (
  echo [提示] 未找到 logo.ico，暂用 north_logo_1.ico
  > resource.rc echo IDI_ICON1 ICON "north_logo_1.ico"
) else (
  > resource.rc echo IDI_ICON1 ICON "logo.ico"
)

echo 使用图标: 
type resource.rc
echo.
echo 正在编译...
call "%VCVARS%" >nul
rc resource.rc
if errorlevel 1 (
  echo [错误] 资源编译失败
  pause
  exit /b 1
)
cl /nologo /EHsc /O2 /MT /utf-8 launcher.cpp resource.res user32.lib advapi32.lib kernel32.lib shell32.lib dxgi.lib d3d11.lib psapi.lib urlmon.lib /Fe:launcher.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 (
  echo [错误] 链接失败
  pause
  exit /b 1
)

echo.
echo [完成] 已生成: %cd%\launcher.exe
echo 下一步: 把项目 Windows 目录拖到 deploy.bat 上即可部署。
echo.
pause
endlocal
