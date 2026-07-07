# UE Game Launcher 启动预检器

适用于 UE4/UE5 Windows 打包项目的 C++ 原生启动预检器（零运行时依赖）。

## 功能

- 启动前检查可用内存、显存、冲突进程（软警告，可强制继续）
- 自动检测并安装 VC++ 运行库、DirectX 11
- 全部业务配置与 UI 文案外置在 `launcher.ini`，无需重新编译即可复用

## 目录说明

| 文件 | 说明 |
|------|------|
| `launcher.cpp` | 源码 |
| `launcher.ini` | 运行时配置（与 exe 同目录） |
| `resource.rc` / `*.ico` | 程序图标 |
| `启动预检器使用说明.docx` | 详细使用文档 |

## 编译

在 VS2022 x64 Native Tools Command Prompt 中：

```bat
rc resource.rc
cl /EHsc /O2 /MT /utf-8 launcher.cpp resource.res user32.lib advapi32.lib kernel32.lib shell32.lib dxgi.lib d3d11.lib psapi.lib urlmon.lib /link /SUBSYSTEM:WINDOWS
```

将生成的 `launcher.exe` 与 `launcher.ini` 复制到 UE 打包的 `Windows\` 目录。

## 快速复用

1. 复制 `launcher.exe` + `launcher.ini` 到新项目 `Windows\` 目录
2. 编辑 `launcher.ini` 中 `[Game] ExePath` 等配置项
3. 无需重新编译（换图标除外）

详细说明见 `启动预检器使用说明.docx`。
