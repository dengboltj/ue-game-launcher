@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

echo ========================================
echo   Launcher 一键部署到项目
echo ========================================
echo.
echo 用法: 把 UE 打包后的 Windows 目录拖到本脚本上
echo       或: deploy.bat "D:\MyGame\Windows"
echo.

set "TARGET=%~1"
if "%TARGET%"=="" (
  echo 请把目标 Windows 文件夹拖到此窗口后按回车:
  set /p "TARGET=路径> "
)
if "%TARGET%"=="" (
  echo [错误] 未指定目标目录
  pause
  exit /b 1
)

set "TARGET=%TARGET:"=%"

if not exist "%TARGET%\" (
  echo [错误] 目标目录不存在: %TARGET%
  pause
  exit /b 1
)

if not exist "%~dp0launcher.exe" (
  echo [错误] 缺少 launcher.exe，请先运行 build.bat
  pause
  exit /b 1
)
if not exist "%~dp0launcher.ini" (
  echo [错误] 缺少 launcher.ini
  pause
  exit /b 1
)

echo 目标目录: %TARGET%
echo.

echo 正在复制 launcher.exe / launcher.ini ...
copy /Y "%~dp0launcher.exe" "%TARGET%\launcher.exe" >nul
copy /Y "%~dp0launcher.ini" "%TARGET%\launcher.ini" >nul

echo 正在自动匹配 *Shipping.exe ...
set "LAUNCHER_TARGET=%TARGET%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$target = [IO.Path]::GetFullPath($env:LAUNCHER_TARGET).TrimEnd('\');" ^
  "$exe = Get-ChildItem -LiteralPath $target -Recurse -Filter '*Shipping.exe' -File -ErrorAction SilentlyContinue |" ^
  "  Where-Object { $_.Name -ne 'launcher.exe' } |" ^
  "  Select-Object -First 1;" ^
  "if (-not $exe) { Write-Host '[警告] 未找到 Shipping.exe，请手动改 launcher.ini 的 ExePath'; exit 0 };" ^
  "$rel = $exe.FullName.Substring($target.Length).TrimStart('\');" ^
  "Write-Host ('找到游戏: ' + $exe.FullName);" ^
  "Write-Host ('相对路径: ' + $rel);" ^
  "$ini = Join-Path $target 'launcher.ini';" ^
  "$c = Get-Content -LiteralPath $ini -Raw -Encoding UTF8;" ^
  "$c = [regex]::Replace($c, '(?m)^(ExePath\s*=).*', ('ExePath=' + $rel));" ^
  "[IO.File]::WriteAllText($ini, $c, [Text.UTF8Encoding]::new($false));" ^
  "Write-Host '已写入 ExePath'"

echo.
echo [完成] 部署成功
echo   %TARGET%\launcher.exe
echo   %TARGET%\launcher.ini
echo.
echo 可直接双击 launcher.exe；改门槛/文案只改 ini，无需重编译。
echo.
pause
endlocal
