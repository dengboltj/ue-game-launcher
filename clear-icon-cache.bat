@echo off
chcp 65001 >nul
echo 正在清理 Windows 图标缓存并重启资源管理器...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
del /f /q "%LOCALAPPDATA%\IconCache.db" >nul 2>&1
del /f /q "%LOCALAPPDATA%\Microsoft\Windows\Explorer\iconcache*" >nul 2>&1
del /f /q "%LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache*" >nul 2>&1
start explorer.exe
echo [完成] 请刷新文件夹查看新图标。
pause
