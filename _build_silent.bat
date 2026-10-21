@echo off
setlocal
cd /d "%~dp0"

set "VCVARS="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    goto :ok
  )
)
echo [错误] 未找到 VS2022
exit /b 1

:ok
if exist "logo.ico" (
  > resource.rc echo IDI_ICON1 ICON "logo.ico"
) else if exist "north_logo_1.ico" (
  > resource.rc echo IDI_ICON1 ICON "north_logo_1.ico"
) else (
  echo [错误] 缺少 logo.ico
  exit /b 1
)

call "%VCVARS%" >nul
rc resource.rc >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /MT /utf-8 launcher.cpp resource.res user32.lib advapi32.lib kernel32.lib shell32.lib dxgi.lib d3d11.lib psapi.lib urlmon.lib /Fe:launcher.exe /link /SUBSYSTEM:WINDOWS >nul
if errorlevel 1 exit /b 1
echo [完成] 编译成功
exit /b 0
