# -*- coding: utf-8 -*-
from pathlib import Path
from docx import Document
from docx.shared import Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
import shutil

root = Path(r"E:\Bag\XY\launcher\launcher")
out_docx = root / "启动预检器使用说明.docx"
out_md = root / "启动预检器使用说明.md"

MD = """# UE 游戏启动预检器 — 使用说明

> 更新日期：2026-10-21（一键部署 + 内存/显存配置版）

## 1. 这是什么

适用于 UE4/UE5 Windows 打包项目的启动预检器（C++ 原生，零运行时依赖）。

玩家双击 `launcher.exe` 后，程序会先做环境检查，再拉起游戏 Shipping.exe。

- 检查可用内存、显存、冲突进程（可强制继续）
- 自动检测并安装 VC++ 运行库、DirectX 11
- 防重开：已启动则提示「程序已经开启」
- 自动忽略 Windows Defender（MsMpEng.exe）等系统常驻进程
- 门槛、文案全部在 `launcher.ini`，多数情况不用重新编译

## 2. 最简单用法（日常只记这一个）

工具目录中的唯一日常入口：

**`一键部署到项目.bat`**

### 2.1 部署到新项目

1. 打开本工具目录（含 `一键部署到项目.bat`、`launcher.ini`、`logo.ico` 等）
2. 把 UE 打包出来的 **`Windows` 文件夹**，拖到 **`一键部署到项目.bat`** 上松开
3. 按提示输入硬件门槛：
   - **最低可用内存** `MinFreeRamGB`（单位 GB，直接回车默认 `32.0`，填 `0` 跳过检查）
   - **最低可用显存** `MinVramGB`（单位 GB，直接回车默认 `16.0`，填 `0` 跳过检查）
4. 等到提示「完成 / 已生成配置」，进入目标 `Windows` 目录，**双击 `launcher.exe`** 启动

脚本会自动：

- 若缺少 `launcher.exe`，或源码/图标比 exe 新 → 自动编译
- 复制 `launcher.exe` + `launcher.ini` 到目标目录
- 自动搜索 `*Shipping.exe`，写入 `ExePath`
- 把你输入的内存/显存写入 `MinFreeRamGB` / `MinVramGB`

### 2.2 玩家如何启动

1. 进入项目的 `Windows` 目录
2. 双击 `launcher.exe`（不要直接点 Shipping.exe）
3. 按提示通过检查后，游戏自动启动

## 3. 换图标

1. 用新图标覆盖工具目录中的 `logo.ico`（必须是 `.ico`）
2. 再把项目的 `Windows` 文件夹拖到 `一键部署到项目.bat` 一次（会自动重编，并再次询问内存/显存）
3. 若仍显示旧图标：双击 `clear-icon-cache.bat`，然后刷新文件夹
4. 仍不对：把 `launcher.exe` 拷到一个新文件夹查看（排除路径缓存）

## 4. 只改配置（不用重新编译）

编辑**目标项目 Windows 目录**里的 `launcher.ini`，保存后再运行 `launcher.exe`。

| 配置项 | 含义 | 示例 |
|--------|------|------|
| `[Game] ExePath` | 游戏相对路径 | `MyGame\\Binaries\\Win64\\MyGame-Win64-Shipping.exe` |
| `MinFreeRamGB` | 最低可用内存(GB)，0=不检查 | `16.0` |
| `MinVramGB` | 最低可用显存(GB)，0=不检查 | `8.0` |
| `CheckDX11` | 是否检查 DX11 | `1` / `0` |
| `AutoInstallDX11` | 缺 DX 时自动安装 | `1` / `0` |
| `AutoInstallVC` | 缺 VC++ 时自动安装 | `1` / `0` |
| `[ConflictProcesses] List` | 冲突进程黑名单 | `360tray.exe,...` |
| `[UI.*]` | 弹窗文案 | 按需修改 |

不要把 `MsMpEng.exe` 写进冲突列表；即便旧 ini 里有，程序也会自动忽略。

部署时已交互写入的 `ExePath` / `MinFreeRamGB` / `MinVramGB`，之后也可随时在 ini 里改。

## 5. 其他脚本（一般不用）

| 文件 | 说明 |
|------|------|
| `一键部署到项目.bat` | **日常唯一入口（推荐）** |
| `build.bat` | 仅手动编译 |
| `deploy.bat` | 仅手动部署（不询问内存/显存） |
| `clear-icon-cache.bat` | 清理 Windows 图标缓存 |
| `_build_silent.bat` | 内部静默编译，勿单独使用 |

## 6. 注意

- `launcher.exe` 与 `launcher.ini` **必须同目录**，部署时不要只拷 exe
- 玩家入口永远是 `launcher.exe`

## 7. 常见问题

**Q：未找到 Shipping.exe？**  
手动改目标目录 `launcher.ini` 的 `ExePath`。

**Q：编译失败？**  
安装 VS2022 并勾选「使用 C++ 的桌面开发」，或使用已编好的 `launcher.exe`。

**Q：还提示 MsMpEng 冲突？**  
旧版 exe/ini 未更新，用一键部署再拖一次覆盖。

**Q：图标不对？**  
换 `logo.ico` 后重新部署，并运行 `clear-icon-cache.bat`。

**Q：提示已经开启？**  
防重开：关闭已运行的启动器/游戏后再开。

**Q：想跳过内存/显存检查？**  
一键部署时对应项填 `0`，或之后把 ini 里改成 `MinFreeRamGB=0` / `MinVramGB=0`。

## 8. 口诀

- 接新项目：拖 Windows → `一键部署到项目.bat` → 输入内存/显存
- 改门槛/文案：只改目标目录 `launcher.ini`
- 换图标：覆盖 `logo.ico` → 再拖一次 → 必要时清缓存
- 玩家入口：永远双击 `launcher.exe`
"""


def build_docx():
    doc = Document()
    style = doc.styles["Normal"]
    style.font.name = "微软雅黑"
    style.font.size = Pt(11)
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

    def h1(t):
        p = doc.add_heading(t, level=1)
        for r in p.runs:
            r.font.name = "微软雅黑"
            r._element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

    def h2(t):
        p = doc.add_heading(t, level=2)
        for r in p.runs:
            r.font.name = "微软雅黑"
            r._element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

    def h3(t):
        p = doc.add_heading(t, level=3)
        for r in p.runs:
            r.font.name = "微软雅黑"
            r._element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

    def para(t):
        doc.add_paragraph(t)

    def bullets(items):
        for i in items:
            doc.add_paragraph(i, style="List Bullet")

    def nums(items):
        for i in items:
            doc.add_paragraph(i, style="List Number")

    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = title.add_run("UE 游戏启动预检器 — 使用说明")
    r.bold = True
    r.font.size = Pt(18)
    r.font.name = "微软雅黑"
    r._element.rPr.rFonts.set(qn("w:eastAsia"), "微软雅黑")

    sub = doc.add_paragraph()
    sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    sr = sub.add_run("更新日期：2026-10-21（一键部署 + 内存/显存配置版）")
    sr.font.size = Pt(10)
    sr.font.color.rgb = RGBColor(0x66, 0x66, 0x66)

    h1("1. 这是什么")
    para("适用于 UE4/UE5 Windows 打包项目的启动预检器（C++ 原生，零运行时依赖）。")
    para("玩家双击 launcher.exe 后，程序会先做环境检查，再拉起游戏 Shipping.exe。")
    bullets([
        "检查可用内存、显存、冲突进程（可强制继续）",
        "自动检测并安装 VC++ 运行库、DirectX 11",
        "防重开：已启动则提示「程序已经开启」",
        "自动忽略 Windows Defender（MsMpEng.exe）等系统常驻进程，避免误报",
        "门槛、文案全部写在 launcher.ini，多数情况不用重新编译",
    ])

    h1("2. 最简单用法（日常只记这一个）")
    para("工具目录中的唯一日常入口：")
    para("    一键部署到项目.bat")

    h2("2.1 部署到新项目")
    nums([
        "打开本工具目录（含 一键部署到项目.bat、launcher.ini、logo.ico 等）",
        "把 UE 打包出来的「Windows」文件夹，拖到「一键部署到项目.bat」上松开",
        "按提示输入最低可用内存、最低可用显存（GB；直接回车用默认 32.0 / 16.0；填 0 跳过检查）",
        "等到提示「完成 / 已生成配置」，进入目标 Windows 目录，双击 launcher.exe 启动",
    ])
    para("脚本会自动完成：")
    bullets([
        "若缺少 launcher.exe，或源码/图标比 exe 新 → 自动编译",
        "复制 launcher.exe + launcher.ini 到目标目录",
        "自动搜索 *Shipping.exe，写入 launcher.ini 的 ExePath",
        "把输入的内存/显存写入 MinFreeRamGB / MinVramGB",
    ])

    h2("2.2 玩家如何启动")
    nums([
        "进入项目的 Windows 目录",
        "双击 launcher.exe（不要直接点 Shipping.exe）",
        "按提示通过检查后，游戏会自动启动",
    ])

    h1("3. 换图标")
    nums([
        "用新图标覆盖工具目录中的 logo.ico（必须是 .ico）",
        "再把项目的 Windows 文件夹拖到「一键部署到项目.bat」一次（会自动重编，并再次询问内存/显存）",
        "若资源管理器仍显示旧图标：双击 clear-icon-cache.bat，然后刷新文件夹",
        "仍不对时：把 launcher.exe 拷到一个新文件夹查看（排除路径缓存）",
    ])

    h1("4. 只改配置（不用重新编译）")
    para("编辑「目标项目 Windows 目录」里的 launcher.ini，保存后直接再运行 launcher.exe。")
    para("部署时已写入的 ExePath / MinFreeRamGB / MinVramGB，之后也可随时在 ini 里改。")

    h2("4.1 常用配置项")
    table = doc.add_table(rows=1, cols=3)
    table.style = "Table Grid"
    hdr = table.rows[0].cells
    hdr[0].text = "配置项"
    hdr[1].text = "含义"
    hdr[2].text = "示例"
    for a, b, c in [
        ("[Game] ExePath", "游戏相对 launcher.exe 的路径", r"MyGame\Binaries\Win64\MyGame-Win64-Shipping.exe"),
        ("MinFreeRamGB", "最低可用内存(GB)，0=不检查", "16.0"),
        ("MinVramGB", "最低可用显存(GB)，0=不检查", "8.0"),
        ("CheckDX11", "是否检查 DirectX 11", "1 或 0"),
        ("AutoInstallDX11", "缺 DX 时是否自动安装", "1 或 0"),
        ("AutoInstallVC", "缺 VC++ 时是否自动安装", "1 或 0"),
        ("[ConflictProcesses] List", "冲突进程黑名单（逗号分隔）", "360tray.exe,..."),
        ("[UI.*]", "弹窗标题与中文文案", "按需修改"),
    ]:
        cells = table.add_row().cells
        cells[0].text = a
        cells[1].text = b
        cells[2].text = c

    para("")
    para("注意：不要把 MsMpEng.exe 写进冲突列表；即便旧 ini 里有，程序也会自动忽略。")

    h1("5. 其他脚本（一般不用）")
    table2 = doc.add_table(rows=1, cols=2)
    table2.style = "Table Grid"
    h = table2.rows[0].cells
    h[0].text = "文件"
    h[1].text = "说明"
    for a, b in [
        ("一键部署到项目.bat", "日常唯一入口（推荐，会询问内存/显存）"),
        ("build.bat", "仅手动编译"),
        ("deploy.bat", "仅手动部署（不询问内存/显存）"),
        ("clear-icon-cache.bat", "清理 Windows 图标缓存"),
        ("_build_silent.bat", "内部静默编译，勿单独使用"),
    ]:
        cells = table2.add_row().cells
        cells[0].text = a
        cells[1].text = b

    h1("6. 目录与产物")
    bullets([
        "工具源码目录：放 bat、cpp、ini、logo.ico、编译出的 launcher.exe",
        "项目运行目录：UE 的 Windows\\ 下应有 launcher.exe + launcher.ini（二者必须同目录）",
        "launcher.exe 与 launcher.ini 必须一起拷贝，不能只拷 exe",
    ])

    h1("7. 常见问题")
    h3("Q1：拖过去提示未找到 Shipping.exe？")
    para("手动编辑目标目录 launcher.ini 的 ExePath，填相对 Windows 目录的游戏路径。")
    h3("Q2：一键部署提示编译失败？")
    para("本机需安装 Visual Studio 2022，并勾选工作负载「使用 C++ 的桌面开发」。或直接使用已编好的 launcher.exe 再部署。")
    h3("Q3：还提示 MsMpEng.exe 冲突？")
    para("说明运行的是旧版 exe，或旧目录未更新。请用「一键部署到项目.bat」重新拖一次 Windows 目录，确保 exe 与 ini 都被覆盖。")
    h3("Q4：图标缩小对、放大错 / 拷走又变回去？")
    para("多为 Windows 图标缓存。换 logo.ico 并重新部署后，运行 clear-icon-cache.bat；或以新路径下的 exe 为准。")
    h3("Q5：提示程序已经开启？")
    para("防重开机制：启动器或游戏已在运行，关闭后再开，或确认任务管理器中无残留进程。")
    h3("Q6：想跳过内存/显存检查？")
    para("一键部署时对应项填 0，或之后把 ini 改成 MinFreeRamGB=0 / MinVramGB=0。")

    h1("8. 口诀")
    bullets([
        "接新项目：拖 Windows → 一键部署到项目.bat → 输入内存/显存",
        "改门槛/文案：只改目标目录 launcher.ini",
        "换图标：覆盖 logo.ico → 再拖一次部署 → 必要时清缓存",
        "玩家入口：永远双击 launcher.exe",
    ])
    return doc


def main():
    out_md.write_text(MD, encoding="utf-8")
    print("md saved:", out_md)

    doc = build_docx()
    try:
        doc.save(out_docx)
        print("docx saved:", out_docx)
    except PermissionError:
        tmp = root / "启动预检器使用说明_新.docx"
        doc.save(tmp)
        print("docx locked, saved as:", tmp)
        print("请关闭已打开的旧 docx 后，把「_新」文件改名为正式文件名。")

    # cleanup duplicate old garbled docx if any other *.docx besides ours
    for d in root.glob("*.docx"):
        if d.name in (out_docx.name, "启动预检器使用说明_新.docx"):
            continue
        # keep only intended names
        if "使用说明" not in d.name and "Ԥ" not in d.name:
            continue


if __name__ == "__main__":
    main()
