@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

echo ========================================
echo   启动器 - 一键部署（拖文件夹即可）
echo ========================================
echo.

rem ========== 1. 拿到目标 Windows 目录 ==========
set "TARGET=%~1"
if "%TARGET%"=="" (
  echo 请把 UE 打包的 Windows 文件夹拖到本窗口，然后回车：
  set /p "TARGET= "
)
set "TARGET=%TARGET:"=%"
if "%TARGET%"=="" (
  echo [错误] 没有目标文件夹。也可直接把文件夹拖到本 bat 图标上。
  pause
  exit /b 1
)
if not exist "%TARGET%\" (
  echo [错误] 文件夹不存在: %TARGET%
  pause
  exit /b 1
)

rem ========== 2. 没有 exe 或图标/源码更新了 → 自动编译 ==========
set "NEED_BUILD=0"
if not exist "%~dp0launcher.exe" set "NEED_BUILD=1"
if exist "%~dp0logo.ico" if exist "%~dp0launcher.exe" (
  powershell -NoProfile -Command ^
    "if ((Get-Item '%~dp0logo.ico').LastWriteTime -gt (Get-Item '%~dp0launcher.exe').LastWriteTime) { exit 9 } else { exit 0 }"
  if errorlevel 9 set "NEED_BUILD=1"
)
if exist "%~dp0launcher.cpp" if exist "%~dp0launcher.exe" (
  powershell -NoProfile -Command ^
    "if ((Get-Item '%~dp0launcher.cpp').LastWriteTime -gt (Get-Item '%~dp0launcher.exe').LastWriteTime) { exit 9 } else { exit 0 }"
  if errorlevel 9 set "NEED_BUILD=1"
)

if "%NEED_BUILD%"=="1" (
  echo [自动] 需要编译 launcher.exe ...
  call "%~dp0_build_silent.bat"
  if errorlevel 1 (
    echo [错误] 编译失败。若本机无 VS2022，请先装「使用 C++ 的桌面开发」，或找已编好的 launcher.exe。
    pause
    exit /b 1
  )
) else (
  echo [跳过] 已有最新 launcher.exe
)

rem ========== 3. 输入内存 / 显存要求 ==========
echo.
echo ----------------------------------------
echo   请输入本项目硬件门槛（单位 GB）
echo   直接回车 = 使用默认值；填 0 = 跳过该项检查
echo ----------------------------------------
set "RAM_DEFAULT=32.0"
set "VRAM_DEFAULT=16.0"
set "RAM_IN="
set "VRAM_IN="
set /p "RAM_IN=最低可用内存 MinFreeRamGB [%RAM_DEFAULT%]: "
set /p "VRAM_IN=最低可用显存 MinVramGB     [%VRAM_DEFAULT%]: "
if "%RAM_IN%"=="" set "RAM_IN=%RAM_DEFAULT%"
if "%VRAM_IN%"=="" set "VRAM_IN=%VRAM_DEFAULT%"
echo [配置] 内存=%RAM_IN% GB  显存=%VRAM_IN% GB
echo.

rem ========== 4. 复制、写配置、删原启动、改名为项目名 ==========
echo [部署] 复制到: %TARGET%
copy /Y "%~dp0launcher.exe" "%TARGET%\launcher.exe" >nul
copy /Y "%~dp0launcher.ini" "%TARGET%\launcher.ini" >nul

set "LAUNCHER_TARGET=%TARGET%"
set "LAUNCHER_RAM=%RAM_IN%"
set "LAUNCHER_VRAM=%VRAM_IN%"
set "LAUNCHER_FINAL_OUT=%TEMP%\launcher_final_exe.txt"
set "FINAL_EXE=launcher.exe"
del /f /q "%LAUNCHER_FINAL_OUT%" >nul 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\deploy_finalize.ps1"
if errorlevel 1 (
  echo [错误] 部署收尾失败，请查看上方报错。
  pause
  exit /b 1
)
if exist "%LAUNCHER_FINAL_OUT%" (
  set /p FINAL_EXE=<"%LAUNCHER_FINAL_OUT%"
)

echo.
echo ========================================
echo   完成！请双击项目启动程序：
echo   %TARGET%\!FINAL_EXE!
echo   （原 UE 启动 exe 已删除，已替换为预检启动器）
echo   配置文件仍为同目录 launcher.ini
echo   内存门槛: %RAM_IN% GB   显存门槛: %VRAM_IN% GB
echo ========================================
echo.
set /p "OPEN=是否打开该文件夹？(Y/N，默认Y): "
if /i not "%OPEN%"=="N" explorer "%TARGET%"
endlocal
