// launcher.cpp - UE5 Game Launch Pre-checker (C++ Native, Zero Dependency)
// Compile in VS2022 x64 Native Tools Command Prompt:
//   rc resource.rc
//   cl /EHsc /O2 /MT /utf-8 launcher.cpp resource.res user32.lib advapi32.lib kernel32.lib shell32.lib dxgi.lib d3d11.lib psapi.lib urlmon.lib /link /SUBSYSTEM:WINDOWS
//
// Configuration: edit launcher.ini next to launcher.exe (no recompile needed)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <urlmon.h>
#include <dxgi1_4.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <vector>
#include <string>
#include <map>

// ============================================================
//  Configuration (loaded from launcher.ini)
// ============================================================

struct LauncherUiConfig {
    std::wstring titleError;
    std::wstring titleWarning;
    std::wstring titleInstallAsk;
    std::wstring titleInstalling;
    std::wstring titleInstallSuccess;
    std::wstring titleInstallFailed;
    std::wstring titleLaunchError;
    std::wstring titleTip;

    std::wstring componentVcShort;
    std::wstring componentVcFull;
    std::wstring componentDx11;
    std::wstring defaultGpuName;

    std::wstring warnLabelRam;
    std::wstring warnLabelVram;
    std::wstring warnLabelConflict;

    std::wstring forceRunSuffix;
    std::wstring askInstall;
    std::wstring installing;
    std::wstring installSuccess;
    std::wstring installFailed;
    std::wstring downloadFailed;
    std::wstring downloadIncomplete;
    std::wstring cannotStartInstaller;
    std::wstring installerErrorCode;
    std::wstring vcRestartRequired;
    std::wstring dxRestartRequired;
    std::wstring dxManualInstallHint;
    std::wstring dxAutoInstallFailed;
    std::wstring dxInstallBlockedHint;
    std::wstring dxInstallVerifyFailed;
    std::wstring d3d11NotFound;
    std::wstring d3d11LoadFailed;
    std::wstring d3d11TooOld;
    std::wstring osTooOld;
    std::wstring ramWarnLine1;
    std::wstring ramWarnLine2;
    std::wstring vramWarnLine1;
    std::wstring vramWarnLine2;
    std::wstring conflictProcPrefix;
    std::wstring exeNotFound;
    std::wstring launchFailed;
    std::wstring combinedWarn;
    std::wstring alreadyRunning;
};

struct LauncherConfig {
    std::wstring exePath;
    double minFreeRamGb;
    double minVramGb;
    bool checkDx11;
    bool autoInstallDx11;
    bool autoInstallVc;
    std::wstring vcRedistUrl;
    std::wstring dx11SetupUrl;
    std::wstring dx11ManualUrl;
    std::vector<std::wstring> conflictProcs;
    LauncherUiConfig ui;
};

static LauncherConfig g_cfg;

typedef std::map<std::wstring, std::map<std::wstring, std::wstring>> IniData;

// ============================================================
//  Config Helpers
// ============================================================

static std::wstring trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r')) a++;
    while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r')) b--;
    return s.substr(a, b - a);
}

static std::string trimUtf8(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, NULL, NULL);
    return s;
}

static std::wstring unescapeIniValue(const std::wstring& s) {
    std::wstring r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            if (s[i + 1] == L'n') { r += L'\n'; i++; continue; }
            if (s[i + 1] == L'\\') { r += L'\\'; i++; continue; }
        }
        r += s[i];
    }
    return r;
}

static std::string escapeIniValue(const std::wstring& s) {
    std::string u8 = wideToUtf8(s);
    std::string r;
    r.reserve(u8.size() + 8);
    for (size_t i = 0; i < u8.size(); i++) {
        if (u8[i] == '\n') { r += "\\n"; continue; }
        if (u8[i] == '\\') { r += "\\\\"; continue; }
        r += u8[i];
    }
    return r;
}

struct ExpandVar {
    const wchar_t* key;
    std::wstring val;
};

static std::wstring expand(const std::wstring& tmpl, const ExpandVar* vars, size_t count) {
    std::wstring r = tmpl;
    for (size_t v = 0; v < count; v++) {
        std::wstring ph = std::wstring(L"{") + vars[v].key + L"}";
        size_t pos = 0;
        while ((pos = r.find(ph, pos)) != std::wstring::npos) {
            r.replace(pos, ph.size(), vars[v].val);
            pos += vars[v].val.size();
        }
    }
    return r;
}

static bool parseBool(const std::wstring& s, bool fallback) {
    std::wstring v = trim(s);
    if (v.empty()) return fallback;
    for (size_t i = 0; i < v.size(); i++)
        v[i] = (wchar_t)::tolower(v[i]);
    if (v == L"1" || v == L"true" || v == L"yes" || v == L"on") return true;
    if (v == L"0" || v == L"false" || v == L"no" || v == L"off") return false;
    return fallback;
}

static double parseDouble(const std::wstring& s, double fallback) {
    std::wstring v = trim(s);
    if (v.empty()) return fallback;
    wchar_t* end = NULL;
    double d = wcstod(v.c_str(), &end);
    if (end == v.c_str()) return fallback;
    return d;
}

static std::vector<std::wstring> splitList(const std::wstring& s) {
    std::vector<std::wstring> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t comma = s.find(L',', start);
        std::wstring item = trim(s.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
        if (!item.empty()) out.push_back(item);
        if (comma == std::wstring::npos) break;
        start = comma + 1;
    }
    return out;
}

static bool readFileUtf8(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) { CloseHandle(h); return false; }
    out.resize(size);
    DWORD read = 0;
    if (!ReadFile(h, size ? &out[0] : NULL, size, &read, NULL)) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    out.resize(read);
    return true;
}

static bool writeFileUtf8(const std::wstring& path, const std::string& content) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    WriteFile(h, bom, 3, &written, NULL);
    if (!content.empty())
        WriteFile(h, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(h);
    return true;
}

static bool parseIniUtf8(const std::string& raw, IniData& out) {
    out.clear();
    size_t pos = 0;
    if (raw.size() >= 3 &&
        (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF)
        pos = 3;

    std::wstring section;
    while (pos < raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string line = trimUtf8(raw.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
        pos = (nl == std::string::npos) ? raw.size() : nl + 1;
        if (line.empty()) continue;
        if (line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end == std::string::npos) return false;
            section = utf8ToWide(line.substr(1, end - 1));
            section = trim(section);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::wstring key = utf8ToWide(trimUtf8(line.substr(0, eq)));
        std::wstring val = unescapeIniValue(utf8ToWide(trimUtf8(line.substr(eq + 1))));
        out[section][key] = val;
    }
    return true;
}

static bool getIniValue(const IniData& ini, const wchar_t* section, const wchar_t* key,
                        std::wstring& out, bool& found) {
    IniData::const_iterator sec = ini.find(section);
    if (sec == ini.end()) { found = false; return true; }
    std::map<std::wstring, std::wstring>::const_iterator it = sec->second.find(key);
    if (it == sec->second.end()) { found = false; return true; }
    out = it->second;
    found = true;
    return true;
}

static void applyStringField(const IniData& ini, const wchar_t* section, const wchar_t* key,
                             std::wstring& field) {
    std::wstring val;
    bool found = false;
    getIniValue(ini, section, key, val, found);
    if (found) field = val;
}

static void applyDefaults(LauncherConfig& cfg) {
    cfg.exePath = L"XYCJ_ZHGDProject\\Binaries\\Win64\\XYCJ_ZHGDProject-Win64-Shipping.exe";
    cfg.minFreeRamGb = 32.0;
    cfg.minVramGb = 16.0;
    cfg.checkDx11 = true;
    cfg.autoInstallDx11 = true;
    cfg.autoInstallVc = true;
    cfg.vcRedistUrl = L"https://aka.ms/vs/17/release/vc_redist.x64.exe";
    cfg.dx11SetupUrl = L"https://go.microsoft.com/fwlink/?LinkID=219729";
    cfg.dx11ManualUrl = L"https://www.microsoft.com/zh-cn/download/details.aspx?id=35";
    // 仅检测第三方安全软件前台/托盘进程；不含 Windows Defender(MsMpEng.exe) 等系统常驻服务
    cfg.conflictProcs = splitList(
        L"360safe.exe,360tray.exe,360sd.exe,360rp.exe,"
        L"qhsafemon.exe,qqpcmgr.exe,qqpcmgrprotect.exe,"
        L"hipstray.exe,hipsdaemon.exe,"
        L"ksafetray.exe,kxetray.exe,avp.exe");

    LauncherUiConfig& u = cfg.ui;
    u.titleError = L"启动失败 - 系统不支持";
    u.titleWarning = L"启动检查 - 警告";
    u.titleInstallAsk = L"环境检查 - 缺少运行组件";
    u.titleInstalling = L"正在安装...";
    u.titleInstallSuccess = L"安装完成";
    u.titleInstallFailed = L"安装失败";
    u.titleLaunchError = L"启动错误";
    u.titleTip = L"提示";

    u.componentVcShort = L"VC++ 运行库";
    u.componentVcFull = L"VC++ 运行库 (Microsoft Visual C++ Redistributable)";
    u.componentDx11 = L"DirectX 11 运行时";
    u.defaultGpuName = L"NVIDIA GPU";

    u.warnLabelRam = L"可用内存不足";
    u.warnLabelVram = L"可用显存不足";
    u.warnLabelConflict = L"检测到冲突进程";

    u.forceRunSuffix = L"\n\n是否仍要强制运行？";
    u.askInstall = L"检测到系统未安装 {component}。\n\n是否自动下载并安装？\n（需要网络连接，可能弹出管理员权限窗口）";
    u.installing = L"正在下载安装 {component}，请稍候...\n\n如果弹出用户账户控制(UAC)窗口，请点击「是」允许安装。";
    u.installSuccess = L"{component} 安装成功！\n点击确定继续启动。";
    u.installFailed = L"{component} 安装失败！\n\n{detail}\n\n请手动安装后重试。";
    u.downloadFailed = L"下载失败，请检查网络连接。\n手动下载地址:\n{url}";
    u.downloadIncomplete = L"下载文件不完整或损坏。";
    u.cannotStartInstaller = L"无法启动安装程序。";
    u.installerErrorCode = L"安装程序返回错误代码。";
    u.vcRestartRequired = L"注意：VC++ 安装要求重启计算机才能生效。\n请重启后再次运行本程序。";
    u.dxRestartRequired = L"DirectX 安装要求重启计算机。\n请重启后再次运行。";
    u.dxManualInstallHint = L"\n\n请手动安装 DirectX 11 End-User Runtime 后重试。";
    u.dxAutoInstallFailed = L"DirectX 11 自动安装失败，游戏无法启动。\n\n请手动下载安装：\n{url}";
    u.dxInstallBlockedHint = L"\n\n请安装 DirectX 11 End-User Runtime 后重试。";
    u.dxInstallVerifyFailed = L"安装完成后仍未通过验证。";
    u.d3d11NotFound = L"未找到 d3d11.dll";
    u.d3d11LoadFailed = L"无法加载 d3d11.dll";
    u.d3d11TooOld = L"d3d11.dll 版本过旧";
    u.osTooOld = L"操作系统版本过低（需要 Win7 SP1 及以上）";
    u.ramWarnLine1 = L"当前内存可用: {avail} GB（总计: {total} GB）";
    u.ramWarnLine2 = L"最低内存要求: {min} GB";
    u.vramWarnLine1 = L"当前可用显存: {avail} GB（总计: {total} GB）";
    u.vramWarnLine2 = L"最低显存要求: {min} GB";
    u.conflictProcPrefix = L"  * ";
    u.exeNotFound = L"未找到程序:\n\n{path}";
    u.launchFailed = L"启动失败! 错误码: {code}";
    u.combinedWarn = L"[!] {title}\n\n{details}建议: 升级硬件，关闭其他程序。\n\n是否仍要强制运行？";
    u.alreadyRunning = L"程序已经开启，请勿重复启动。";
}

static void writeIniLine(std::string& out, const char* comment) {
    out += comment;
    out += "\r\n";
}

static void writeIniKey(std::string& out, const char* key, const std::wstring& val) {
    out += key;
    out += "=";
    out += escapeIniValue(val);
    out += "\r\n";
}

static bool writeConfigFile(const std::wstring& path, const LauncherConfig& cfg) {
    std::string out;
    writeIniLine(out, "; UE5 Game Launcher 配置文件");
    writeIniLine(out, "; 放置在与 launcher.exe 相同目录，修改后无需重新编译");
    out += "\r\n";

    out += "[Game]\r\n";
    writeIniLine(out, "; 游戏 exe 相对 launcher.exe 的路径");
    writeIniKey(out, "ExePath", cfg.exePath);
    out += "\r\n";

    out += "[Requirements]\r\n";
    writeIniLine(out, "; 最低可用物理内存(GB)，0=跳过检查");
    wchar_t numBuf[64];
    swprintf(numBuf, 64, L"%.1f", cfg.minFreeRamGb);
    writeIniKey(out, "MinFreeRamGB", numBuf);
    writeIniLine(out, "; 最低可用显存(GB)，0=跳过检查");
    swprintf(numBuf, 64, L"%.1f", cfg.minVramGb);
    writeIniKey(out, "MinVramGB", numBuf);
    out += "\r\n";

    out += "[Environment]\r\n";
    writeIniLine(out, "; 是否检查 DirectX 11（0/1）");
    writeIniKey(out, "CheckDX11", cfg.checkDx11 ? L"1" : L"0");
    writeIniLine(out, "; 缺失时是否自动下载安装 DX11（0/1）");
    writeIniKey(out, "AutoInstallDX11", cfg.autoInstallDx11 ? L"1" : L"0");
    writeIniLine(out, "; 缺失时是否自动下载安装 VC++ 运行库（0/1）");
    writeIniKey(out, "AutoInstallVC", cfg.autoInstallVc ? L"1" : L"0");
    writeIniLine(out, "; VC++ x64 运行库下载地址");
    writeIniKey(out, "VCRedistUrl", cfg.vcRedistUrl);
    writeIniLine(out, "; DirectX Web Setup 下载地址");
    writeIniKey(out, "DX11SetupUrl", cfg.dx11SetupUrl);
    writeIniLine(out, "; DirectX 手动下载页面（自动安装失败时提示）");
    writeIniKey(out, "DX11ManualUrl", cfg.dx11ManualUrl);
    out += "\r\n";

    out += "[ConflictProcesses]\r\n";
    writeIniLine(out, "; 冲突进程黑名单，逗号分隔（小写 exe 名）；留空=跳过检测");
    std::wstring conflictList;
    for (size_t i = 0; i < cfg.conflictProcs.size(); i++) {
        if (i > 0) conflictList += L",";
        conflictList += cfg.conflictProcs[i];
    }
    writeIniKey(out, "List", conflictList);
    out += "\r\n";

    out += "[UI.Titles]\r\n";
    writeIniKey(out, "ErrorTitle", cfg.ui.titleError);
    writeIniKey(out, "WarningTitle", cfg.ui.titleWarning);
    writeIniKey(out, "InstallAskTitle", cfg.ui.titleInstallAsk);
    writeIniKey(out, "InstallingTitle", cfg.ui.titleInstalling);
    writeIniKey(out, "InstallSuccessTitle", cfg.ui.titleInstallSuccess);
    writeIniKey(out, "InstallFailedTitle", cfg.ui.titleInstallFailed);
    writeIniKey(out, "LaunchErrorTitle", cfg.ui.titleLaunchError);
    writeIniKey(out, "TipTitle", cfg.ui.titleTip);
    out += "\r\n";

    out += "[UI.Components]\r\n";
    writeIniKey(out, "VcShort", cfg.ui.componentVcShort);
    writeIniKey(out, "VcFull", cfg.ui.componentVcFull);
    writeIniKey(out, "Dx11", cfg.ui.componentDx11);
    writeIniKey(out, "DefaultGpuName", cfg.ui.defaultGpuName);
    out += "\r\n";

    out += "[UI.WarningLabels]\r\n";
    writeIniKey(out, "Ram", cfg.ui.warnLabelRam);
    writeIniKey(out, "Vram", cfg.ui.warnLabelVram);
    writeIniKey(out, "Conflict", cfg.ui.warnLabelConflict);
    out += "\r\n";

    out += "[UI.Messages]\r\n";
    writeIniKey(out, "ForceRunSuffix", cfg.ui.forceRunSuffix);
    writeIniKey(out, "AskInstall", cfg.ui.askInstall);
    writeIniKey(out, "Installing", cfg.ui.installing);
    writeIniKey(out, "InstallSuccess", cfg.ui.installSuccess);
    writeIniKey(out, "InstallFailed", cfg.ui.installFailed);
    writeIniKey(out, "DownloadFailed", cfg.ui.downloadFailed);
    writeIniKey(out, "DownloadIncomplete", cfg.ui.downloadIncomplete);
    writeIniKey(out, "CannotStartInstaller", cfg.ui.cannotStartInstaller);
    writeIniKey(out, "InstallerErrorCode", cfg.ui.installerErrorCode);
    writeIniKey(out, "VcRestartRequired", cfg.ui.vcRestartRequired);
    writeIniKey(out, "DxRestartRequired", cfg.ui.dxRestartRequired);
    writeIniKey(out, "DxManualInstallHint", cfg.ui.dxManualInstallHint);
    writeIniKey(out, "DxAutoInstallFailed", cfg.ui.dxAutoInstallFailed);
    writeIniKey(out, "DxInstallBlockedHint", cfg.ui.dxInstallBlockedHint);
    writeIniKey(out, "DxInstallVerifyFailed", cfg.ui.dxInstallVerifyFailed);
    writeIniKey(out, "D3D11NotFound", cfg.ui.d3d11NotFound);
    writeIniKey(out, "D3D11LoadFailed", cfg.ui.d3d11LoadFailed);
    writeIniKey(out, "D3D11TooOld", cfg.ui.d3d11TooOld);
    writeIniKey(out, "OsTooOld", cfg.ui.osTooOld);
    writeIniKey(out, "RamWarnLine1", cfg.ui.ramWarnLine1);
    writeIniKey(out, "RamWarnLine2", cfg.ui.ramWarnLine2);
    writeIniKey(out, "VramWarnLine1", cfg.ui.vramWarnLine1);
    writeIniKey(out, "VramWarnLine2", cfg.ui.vramWarnLine2);
    writeIniKey(out, "ConflictProcPrefix", cfg.ui.conflictProcPrefix);
    writeIniKey(out, "ExeNotFound", cfg.ui.exeNotFound);
    writeIniKey(out, "LaunchFailed", cfg.ui.launchFailed);
    writeIniKey(out, "CombinedWarn", cfg.ui.combinedWarn);
    writeIniKey(out, "AlreadyRunning", cfg.ui.alreadyRunning);
    out += "\r\n";

    return writeFileUtf8(path, out);
}

static bool loadConfig(const std::wstring& iniPath) {
    applyDefaults(g_cfg);

    std::string raw;
    if (!readFileUtf8(iniPath, raw))
        return false;

    IniData ini;
    if (!parseIniUtf8(raw, ini))
        return false;

    applyStringField(ini, L"Game", L"ExePath", g_cfg.exePath);

    std::wstring val;
    bool found = false;
    getIniValue(ini, L"Requirements", L"MinFreeRamGB", val, found);
    if (found) g_cfg.minFreeRamGb = parseDouble(val, g_cfg.minFreeRamGb);
    getIniValue(ini, L"Requirements", L"MinVramGB", val, found);
    if (found) g_cfg.minVramGb = parseDouble(val, g_cfg.minVramGb);

    getIniValue(ini, L"Environment", L"CheckDX11", val, found);
    if (found) g_cfg.checkDx11 = parseBool(val, g_cfg.checkDx11);
    getIniValue(ini, L"Environment", L"AutoInstallDX11", val, found);
    if (found) g_cfg.autoInstallDx11 = parseBool(val, g_cfg.autoInstallDx11);
    getIniValue(ini, L"Environment", L"AutoInstallVC", val, found);
    if (found) g_cfg.autoInstallVc = parseBool(val, g_cfg.autoInstallVc);
    applyStringField(ini, L"Environment", L"VCRedistUrl", g_cfg.vcRedistUrl);
    applyStringField(ini, L"Environment", L"DX11SetupUrl", g_cfg.dx11SetupUrl);
    applyStringField(ini, L"Environment", L"DX11ManualUrl", g_cfg.dx11ManualUrl);

    getIniValue(ini, L"ConflictProcesses", L"List", val, found);
    if (found) g_cfg.conflictProcs = splitList(val);

    LauncherUiConfig& u = g_cfg.ui;
    applyStringField(ini, L"UI.Titles", L"ErrorTitle", u.titleError);
    applyStringField(ini, L"UI.Titles", L"WarningTitle", u.titleWarning);
    applyStringField(ini, L"UI.Titles", L"InstallAskTitle", u.titleInstallAsk);
    applyStringField(ini, L"UI.Titles", L"InstallingTitle", u.titleInstalling);
    applyStringField(ini, L"UI.Titles", L"InstallSuccessTitle", u.titleInstallSuccess);
    applyStringField(ini, L"UI.Titles", L"InstallFailedTitle", u.titleInstallFailed);
    applyStringField(ini, L"UI.Titles", L"LaunchErrorTitle", u.titleLaunchError);
    applyStringField(ini, L"UI.Titles", L"TipTitle", u.titleTip);

    applyStringField(ini, L"UI.Components", L"VcShort", u.componentVcShort);
    applyStringField(ini, L"UI.Components", L"VcFull", u.componentVcFull);
    applyStringField(ini, L"UI.Components", L"Dx11", u.componentDx11);
    applyStringField(ini, L"UI.Components", L"DefaultGpuName", u.defaultGpuName);

    applyStringField(ini, L"UI.WarningLabels", L"Ram", u.warnLabelRam);
    applyStringField(ini, L"UI.WarningLabels", L"Vram", u.warnLabelVram);
    applyStringField(ini, L"UI.WarningLabels", L"Conflict", u.warnLabelConflict);

    applyStringField(ini, L"UI.Messages", L"ForceRunSuffix", u.forceRunSuffix);
    applyStringField(ini, L"UI.Messages", L"AskInstall", u.askInstall);
    applyStringField(ini, L"UI.Messages", L"Installing", u.installing);
    applyStringField(ini, L"UI.Messages", L"InstallSuccess", u.installSuccess);
    applyStringField(ini, L"UI.Messages", L"InstallFailed", u.installFailed);
    applyStringField(ini, L"UI.Messages", L"DownloadFailed", u.downloadFailed);
    applyStringField(ini, L"UI.Messages", L"DownloadIncomplete", u.downloadIncomplete);
    applyStringField(ini, L"UI.Messages", L"CannotStartInstaller", u.cannotStartInstaller);
    applyStringField(ini, L"UI.Messages", L"InstallerErrorCode", u.installerErrorCode);
    applyStringField(ini, L"UI.Messages", L"VcRestartRequired", u.vcRestartRequired);
    applyStringField(ini, L"UI.Messages", L"DxRestartRequired", u.dxRestartRequired);
    applyStringField(ini, L"UI.Messages", L"DxManualInstallHint", u.dxManualInstallHint);
    applyStringField(ini, L"UI.Messages", L"DxAutoInstallFailed", u.dxAutoInstallFailed);
    applyStringField(ini, L"UI.Messages", L"DxInstallBlockedHint", u.dxInstallBlockedHint);
    applyStringField(ini, L"UI.Messages", L"DxInstallVerifyFailed", u.dxInstallVerifyFailed);
    applyStringField(ini, L"UI.Messages", L"D3D11NotFound", u.d3d11NotFound);
    applyStringField(ini, L"UI.Messages", L"D3D11LoadFailed", u.d3d11LoadFailed);
    applyStringField(ini, L"UI.Messages", L"D3D11TooOld", u.d3d11TooOld);
    applyStringField(ini, L"UI.Messages", L"OsTooOld", u.osTooOld);
    applyStringField(ini, L"UI.Messages", L"RamWarnLine1", u.ramWarnLine1);
    applyStringField(ini, L"UI.Messages", L"RamWarnLine2", u.ramWarnLine2);
    applyStringField(ini, L"UI.Messages", L"VramWarnLine1", u.vramWarnLine1);
    applyStringField(ini, L"UI.Messages", L"VramWarnLine2", u.vramWarnLine2);
    applyStringField(ini, L"UI.Messages", L"ConflictProcPrefix", u.conflictProcPrefix);
    applyStringField(ini, L"UI.Messages", L"ExeNotFound", u.exeNotFound);
    applyStringField(ini, L"UI.Messages", L"LaunchFailed", u.launchFailed);
    applyStringField(ini, L"UI.Messages", L"CombinedWarn", u.combinedWarn);
    applyStringField(ini, L"UI.Messages", L"AlreadyRunning", u.alreadyRunning);

    return true;
}

std::wstring getExeDir();

static void ensureConfigFile() {
    std::wstring path = getExeDir() + L"\\launcher.ini";
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return;
    LauncherConfig defaults;
    applyDefaults(defaults);
    writeConfigFile(path, defaults);
}

// ============================================================
//  Utility Functions
// ============================================================

std::wstring getExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? p.substr(0, pos) : L".";
}

std::wstring fmtGB(double gb) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%.1f", gb);
    return std::wstring(buf);
}

std::wstring toLower(const std::wstring& s) {
    std::wstring r = s;
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (wchar_t)::tolower(r[i]);
    return r;
}

std::wstring getTempPath() {
    wchar_t buf[MAX_PATH];
    ::GetTempPathW(MAX_PATH, buf);
    return std::wstring(buf);
}

// ============================================================
//  Dialog Helpers
// ============================================================

void showError(const std::wstring& msg) {
    MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleError.c_str(), MB_OK | MB_ICONERROR);
}

bool askForceRun(const std::wstring& msg) {
    std::wstring full = msg + g_cfg.ui.forceRunSuffix;
    int ret = MessageBoxW(NULL, full.c_str(), g_cfg.ui.titleWarning.c_str(), MB_YESNO | MB_ICONWARNING);
    return (ret == IDYES);
}

bool askInstall(const std::wstring& component) {
    ExpandVar vars[] = { { L"component", component } };
    std::wstring msg = expand(g_cfg.ui.askInstall, vars, 1);
    int ret = MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleInstallAsk.c_str(), MB_YESNO | MB_ICONQUESTION);
    return (ret == IDYES);
}

void showInstalling(const std::wstring& component) {
    ExpandVar vars[] = { { L"component", component } };
    std::wstring msg = expand(g_cfg.ui.installing, vars, 1);
    MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleInstalling.c_str(), MB_OK | MB_ICONINFORMATION);
}

void showInstallSuccess(const std::wstring& component) {
    ExpandVar vars[] = { { L"component", component } };
    std::wstring msg = expand(g_cfg.ui.installSuccess, vars, 1);
    MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleInstallSuccess.c_str(), MB_OK | MB_ICONINFORMATION);
}

void showInstallFailed(const std::wstring& component, const std::wstring& detail) {
    ExpandVar vars[] = {
        { L"component", component },
        { L"detail", detail }
    };
    std::wstring msg = expand(g_cfg.ui.installFailed, vars, 2);
    MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleInstallFailed.c_str(), MB_OK | MB_ICONERROR);
}

// ============================================================
//  Download Helper (using URLDownloadToFileW from urlmon.dll)
// ============================================================

typedef HRESULT (WINAPI* PFN_URLDownloadToFileW)(LPCWSTR, LPCWSTR, DWORD, LPBINDSTATUSCALLBACK);

bool downloadFile(const wchar_t* url, const std::wstring& savePath) {
    HMODULE hUrlMon = LoadLibraryW(L"urlmon.dll");
    if (!hUrlMon) return false;

    PFN_URLDownloadToFileW pDownload =
        (PFN_URLDownloadToFileW)GetProcAddress(hUrlMon, "URLDownloadToFileW");
    if (!pDownload) { FreeLibrary(hUrlMon); return false; }

    HRESULT hr = pDownload(url, savePath.c_str(), 0, NULL);
    FreeLibrary(hUrlMon);

    return SUCCEEDED(hr);
}

// ============================================================
//  VC++ Redistributable Detection & Auto-Install
// ============================================================

bool isVCRedistInstalled() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring dllPath = std::wstring(sysDir) + L"\\vcruntime140.dll";
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;

    HKEY hKey;
    LONG regErr = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64",
        0, KEY_READ, &hKey);
    if (regErr == ERROR_SUCCESS) {
        wchar_t verBuf[64] = {0};
        DWORD len = sizeof(verBuf) - sizeof(wchar_t);
        DWORD type = 0;
        RegQueryValueExW(hKey, L"Version", 0, &type, (LPBYTE)verBuf, &len);
        RegCloseKey(hKey);
        if (verBuf[0] != L'\0')
            return true;
    }

    return true;
}

bool installVCRedist() {
    const std::wstring& vcShort = g_cfg.ui.componentVcShort;
    std::wstring tmpDir = getTempPath();
    std::wstring installerPath = tmpDir + L"vc_redist_x64_installer.exe";

    DeleteFileW(installerPath.c_str());

    showInstalling(g_cfg.ui.componentVcFull);

    if (!downloadFile(g_cfg.vcRedistUrl.c_str(), installerPath)) {
        ExpandVar vars[] = {
            { L"url", g_cfg.vcRedistUrl }
        };
        showInstallFailed(vcShort, expand(g_cfg.ui.downloadFailed, vars, 1));
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fattr = {};
    if (!GetFileAttributesExW(installerPath.c_str(), GetFileExInfoStandard, &fattr) ||
        ((unsigned __int64)fattr.nFileSizeHigh << 32 | fattr.nFileSizeLow) < 10 * 1024 * 1024) {
        showInstallFailed(vcShort, g_cfg.ui.downloadIncomplete);
        DeleteFileW(installerPath.c_str());
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = installerPath.c_str();
    sei.lpParameters = L"/passive /norestart";
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            sei.lpVerb = L"open";
            if (!ShellExecuteExW(&sei)) {
                showInstallFailed(vcShort, g_cfg.ui.cannotStartInstaller);
                DeleteFileW(installerPath.c_str());
                return false;
            }
        } else {
            showInstallFailed(vcShort, g_cfg.ui.cannotStartInstaller);
            DeleteFileW(installerPath.c_str());
            return false;
        }
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 300000);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);

        DeleteFileW(installerPath.c_str());

        if (exitCode == 0 || exitCode == 3010) {
            showInstallSuccess(vcShort);
            if (exitCode == 3010) {
                MessageBoxW(NULL, g_cfg.ui.vcRestartRequired.c_str(),
                            g_cfg.ui.titleTip.c_str(), MB_OK | MB_ICONINFORMATION);
                return false;
            }
            return true;
        } else {
            showInstallFailed(vcShort, g_cfg.ui.installerErrorCode);
            return false;
        }
    }

    DeleteFileW(installerPath.c_str());
    return false;
}

bool checkAndInstallVC() {
    if (!g_cfg.autoInstallVc) return true;

    if (isVCRedistInstalled())
        return true;

    if (!askInstall(g_cfg.ui.componentVcFull))
        return false;

    return installVCRedist();
}

// ============================================================
//  DirectX 11 Detection & Auto-Install
// ============================================================

bool checkDX11Basic(std::wstring& errMsg) {
    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    std::wstring d3d11 = std::wstring(sysPath) + L"\\d3d11.dll";

    if (GetFileAttributesW(d3d11.c_str()) == INVALID_FILE_ATTRIBUTES) {
        errMsg = g_cfg.ui.d3d11NotFound;
        return false;
    }

    HMODULE hD3D11 = LoadLibraryW(L"d3d11.dll");
    if (!hD3D11) {
        errMsg = g_cfg.ui.d3d11LoadFailed;
        return false;
    }
    FARPROC pfn = GetProcAddress(hD3D11, "D3D11CreateDevice");
    FreeLibrary(hD3D11);
    if (!pfn) {
        errMsg = g_cfg.ui.d3d11TooOld;
        return false;
    }

    OSVERSIONINFOEXW vi = { sizeof(vi) };
    GetVersionExW((OSVERSIONINFOW*)&vi);
    if (vi.dwMajorVersion < 6 || (vi.dwMajorVersion == 6 && vi.dwMinorVersion < 1)) {
        errMsg = g_cfg.ui.osTooOld;
        return false;
    }
    return true;
}

bool installDX11Runtime() {
    const std::wstring& dxName = g_cfg.ui.componentDx11;
    std::wstring tmpDir = getTempPath();
    std::wstring installerPath = tmpDir + L"dxwebsetup.exe";

    DeleteFileW(installerPath.c_str());

    showInstalling(dxName);

    if (!downloadFile(g_cfg.dx11SetupUrl.c_str(), installerPath)) {
        ExpandVar vars[] = { { L"url", g_cfg.dx11SetupUrl } };
        showInstallFailed(dxName, expand(g_cfg.ui.downloadFailed, vars, 1));
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fattr = {};
    if (!GetFileAttributesExW(installerPath.c_str(), GetFileExInfoStandard, &fattr) ||
        ((unsigned __int64)fattr.nFileSizeHigh << 32 | fattr.nFileSizeLow) < 100 * 1024) {
        showInstallFailed(dxName, g_cfg.ui.downloadIncomplete);
        DeleteFileW(installerPath.c_str());
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = installerPath.c_str();
    sei.lpParameters = L"/Q";
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            sei.lpVerb = L"open";
            if (!ShellExecuteExW(&sei)) {
                showInstallFailed(dxName, g_cfg.ui.cannotStartInstaller);
                DeleteFileW(installerPath.c_str());
                return false;
            }
        } else {
            showInstallFailed(dxName, g_cfg.ui.cannotStartInstaller);
            DeleteFileW(installerPath.c_str());
            return false;
        }
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 600000);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        DeleteFileW(installerPath.c_str());

        if (exitCode == 0 || exitCode == 3010) {
            showInstallSuccess(dxName);
            if (exitCode == 3010) {
                MessageBoxW(NULL, g_cfg.ui.dxRestartRequired.c_str(),
                            g_cfg.ui.titleTip.c_str(), MB_OK | MB_ICONINFORMATION);
                return false;
            }
            return true;
        } else {
            std::wstring recheck;
            if (checkDX11Basic(recheck))
                return true;
            showInstallFailed(dxName, g_cfg.ui.dxInstallVerifyFailed);
            return false;
        }
    }

    DeleteFileW(installerPath.c_str());
    return false;
}

bool checkDX11(std::wstring& errMsg) {
    if (!g_cfg.checkDx11) return true;

    if (checkDX11Basic(errMsg))
        return true;

    if (g_cfg.autoInstallDx11) {
        if (!askInstall(g_cfg.ui.componentDx11)) {
            errMsg += g_cfg.ui.dxManualInstallHint;
            showError(errMsg);
            ExitProcess(1);
        }

        if (installDX11Runtime()) {
            return true;
        } else {
            ExpandVar vars[] = { { L"url", g_cfg.dx11ManualUrl } };
            errMsg = expand(g_cfg.ui.dxAutoInstallFailed, vars, 1);
            showError(errMsg);
            ExitProcess(1);
        }
    }

    errMsg += g_cfg.ui.dxInstallBlockedHint;
    return false;
}

// ============================================================
//  RAM Check (SOFT WARNING)
// ============================================================

bool checkRAM(std::wstring& warnMsg) {
    if (g_cfg.minFreeRamGb <= 0.0) return false;

    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (!GlobalMemoryStatusEx(&mem)) return false;

    double totalGB = (double)mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
    double availGB = (double)mem.ullAvailPhys / (1024.0 * 1024.0 * 1024.0);

    if (availGB < g_cfg.minFreeRamGb) {
        ExpandVar vars1[] = {
            { L"avail", fmtGB(availGB) },
            { L"total", fmtGB(totalGB) }
        };
        ExpandVar vars2[] = { { L"min", fmtGB(g_cfg.minFreeRamGb) } };
        warnMsg += expand(g_cfg.ui.ramWarnLine1, vars1, 2) + L"\n";
        warnMsg += expand(g_cfg.ui.ramWarnLine2, vars2, 1);
        return true;
    }
    return false;
}

// ============================================================
//  VRAM Check via NVML (Primary) -> DXGI Desc (Fallback)
// ============================================================

struct VramInfo {
    std::wstring name;
    double freeGB;
    double totalGB;
};

struct NvmlMemV1 {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct NvmlMemV2 {
    unsigned long long version;
    unsigned long long total;
    unsigned long long reserved;
    unsigned long long free;
};

typedef int (__stdcall* PFN_nvmlInit)(void);
typedef int (__stdcall* PFN_nvmlShutdown)(void);
typedef int (__stdcall* PFN_nvmlDeviceGetCount)(unsigned int*);
typedef int (__stdcall* PFN_nvmlDeviceGetHandleByIndex)(unsigned int, void**);
typedef int (__stdcall* PFN_nvmlDeviceGetName)(void*, char*, unsigned int);
typedef int (__stdcall* PFN_nvmlDeviceGetMemoryInfo)(void*, NvmlMemV1*);
typedef int (__stdcall* PFN_nvmlDeviceGetMemoryInfo_v2)(void*, NvmlMemV2*);

bool checkVRAM_NVML(std::vector<VramInfo>& results) {
    HMODULE hNVML = LoadLibraryW(L"nvml.dll");
    if (!hNVML) return false;

    auto pInit  = (PFN_nvmlInit)GetProcAddress(hNVML, "nvmlInit_v2");
    if (!pInit) pInit = (PFN_nvmlInit)GetProcAddress(hNVML, "nvmlInit");
    auto pCount = (PFN_nvmlDeviceGetCount)GetProcAddress(hNVML, "nvmlDeviceGetCount");
    auto pHdl   = (PFN_nvmlDeviceGetHandleByIndex)GetProcAddress(hNVML, "nvmlDeviceGetHandleByIndex");
    auto pName  = (PFN_nvmlDeviceGetName)GetProcAddress(hNVML, "nvmlDeviceGetName");
    auto pMemV2 = (PFN_nvmlDeviceGetMemoryInfo_v2)GetProcAddress(hNVML, "nvmlDeviceGetMemoryInfo_v2");
    auto pMemV1 = (PFN_nvmlDeviceGetMemoryInfo)GetProcAddress(hNVML, "nvmlDeviceGetMemoryInfo");
    auto pShut  = (PFN_nvmlShutdown)GetProcAddress(hNVML, "nvmlShutdown");

    if (!pInit || !pCount || !pHdl || !pName || (!pMemV1 && !pMemV2) || !pShut) {
        FreeLibrary(hNVML);
        return false;
    }

    if (pInit() != 0) { FreeLibrary(hNVML); return false; }

    unsigned int gpuCount = 0;
    pCount(&gpuCount);

    for (unsigned int i = 0; i < gpuCount; i++) {
        void* hDev = nullptr;
        if (pHdl(i, &hDev) != 0) continue;

        char nameBuf[256] = {};
        pName(hDev, nameBuf, sizeof(nameBuf));

        VramInfo info;
        info.freeGB = -1.0;

        int wlen = MultiByteToWideChar(CP_ACP, 0, nameBuf, -1, NULL, 0);
        if (wlen > 0) {
            std::vector<wchar_t> wbuf(wlen);
            MultiByteToWideChar(CP_ACP, 0, nameBuf, -1, wbuf.data(), wlen);
            info.name = std::wstring(wbuf.data());
        } else {
            info.name = g_cfg.ui.defaultGpuName;
        }

        if (pMemV2) {
            NvmlMemV2 mem = {};
            if (pMemV2(hDev, &mem) == 0) {
                info.totalGB = (double)mem.total / (1024.0 * 1024.0 * 1024.0);
                info.freeGB  = (double)mem.free  / (1024.0 * 1024.0 * 1024.0);
            }
        }

        if (info.freeGB < 0 && pMemV1) {
            NvmlMemV1 mem = {};
            if (pMemV1(hDev, &mem) == 0) {
                info.totalGB = (double)mem.total / (1024.0 * 1024.0 * 1024.0);
                info.freeGB  = (double)mem.free  / (1024.0 * 1024.0 * 1024.0);
            }
        }

        if (info.freeGB >= 0)
            results.push_back(info);
    }

    pShut();
    FreeLibrary(hNVML);
    return !results.empty();
}

bool checkVRAM_DXGI_Fallback(std::vector<VramInfo>& results) {
    IDXGIFactory4* pFactory = nullptr;
    HRESULT hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), (void**)&pFactory);
    if (FAILED(hr)) return false;

    UINT idx = 0;
    while (true) {
        IDXGIAdapter* pAdapt = nullptr;
        hr = pFactory->EnumAdapters(idx, &pAdapt);
        if (hr == DXGI_ERROR_NOT_FOUND || !pAdapt) break;
        if (FAILED(hr)) { idx++; continue; }

        DXGI_ADAPTER_DESC desc = {};
        pAdapt->GetDesc(&desc);

        if (desc.VendorId != 0x1414 && desc.DedicatedVideoMemory > 0) {
            VramInfo info;
            wchar_t nameBuf[256] = {};
            wcsncpy(nameBuf, desc.Description, 255);
            info.name = std::wstring(nameBuf);
            info.totalGB = (double)desc.DedicatedVideoMemory / (1024.0 * 1024.0 * 1024.0);
            info.freeGB = -1.0;
            results.push_back(info);
        }
        pAdapt->Release();
        idx++;
    }
    pFactory->Release();
    return !results.empty();
}

bool checkVRAM(std::wstring& warnMsg) {
    if (g_cfg.minVramGb <= 0.0) return false;

    std::vector<VramInfo> results;

    bool hasAccurateFree = checkVRAM_NVML(results);

    if (!hasAccurateFree)
        checkVRAM_DXGI_Fallback(results);

    if (results.empty()) return false;

    int bestIdx = 0;
    for (size_t i = 1; i < results.size(); i++)
        if (results[i].totalGB > results[bestIdx].totalGB) bestIdx = i;
    const VramInfo& b = results[(size_t)bestIdx];

    if (b.freeGB >= 0 && b.freeGB < g_cfg.minVramGb) {
        ExpandVar vars1[] = {
            { L"avail", fmtGB(b.freeGB) },
            { L"total", fmtGB(b.totalGB) }
        };
        ExpandVar vars2[] = { { L"min", fmtGB(g_cfg.minVramGb) } };
        warnMsg += expand(g_cfg.ui.vramWarnLine1, vars1, 2) + L"\n";
        warnMsg += expand(g_cfg.ui.vramWarnLine2, vars2, 1);
        return true;
    }
    return false;
}

// ============================================================
//  Conflict Process Check (SOFT WARNING)
// ============================================================

static bool isIgnoredConflictProc(const std::wstring& exeLower) {
    // Always-on system / AV service processes — never treat as user conflicts,
    // even if an old launcher.ini still lists them.
    static const wchar_t* kIgnore[] = {
        L"msmpeng.exe", L"mssense.exe", L"securityhealthservice.exe",
        L"smartscreen.exe", L"avgnt.exe", L"avguard.exe", L"tray.exe"
    };
    for (size_t i = 0; i < sizeof(kIgnore) / sizeof(kIgnore[0]); i++) {
        if (exeLower == kIgnore[i]) return true;
    }
    return false;
}

bool checkConflicts(std::wstring& warnMsg) {
    if (g_cfg.conflictProcs.empty()) return false;

    std::vector<std::wstring> found;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            std::wstring exe = toLower(pe.szExeFile);
            if (isIgnoredConflictProc(exe)) continue;
            for (size_t i = 0; i < g_cfg.conflictProcs.size(); i++) {
                if (exe == toLower(g_cfg.conflictProcs[i])) {
                    found.push_back(std::wstring(pe.szExeFile));
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (!found.empty()) {
        for (const auto& s : found)
            warnMsg += g_cfg.ui.conflictProcPrefix + s + L"\n";
        return true;
    }
    return false;
}

// ============================================================
//  Single Instance Guard
// ============================================================

static std::wstring getFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

static bool isProcessRunning(const std::wstring& exeName) {
    if (exeName.empty()) return false;
    std::wstring target = toLower(exeName);
    DWORD selfPid = GetCurrentProcessId();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == selfPid) continue;
            if (toLower(pe.szExeFile) == target) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

static void showAlreadyRunning() {
    MessageBoxW(NULL, g_cfg.ui.alreadyRunning.c_str(),
                g_cfg.ui.titleTip.c_str(), MB_OK | MB_ICONINFORMATION);
}

// Returns false if another instance is already running (caller should exit).
static bool acquireSingleInstance() {
    // Named mutex: blocks concurrent launcher.exe instances during checks.
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\XYCJ_GameLauncher_SingleInstance");
    if (hMutex == NULL)
        return true; // fail open: allow launch if mutex creation fails

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        showAlreadyRunning();
        return false;
    }
    // Keep mutex handle open for process lifetime (no CloseHandle).

    // Also block if the target game process is already running
    // (launcher exits after CreateProcess, so mutex alone is not enough).
    std::wstring gameExe = getFileName(g_cfg.exePath);
    if (isProcessRunning(gameExe)) {
        showAlreadyRunning();
        return false;
    }
    return true;
}

// ============================================================
//  Launch Game
// ============================================================

bool launchGame() {
    std::wstring exeDir = getExeDir();
    std::wstring exePath = exeDir + L"\\" + g_cfg.exePath;

    if (GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ExpandVar vars[] = { { L"path", exePath } };
        std::wstring msg = expand(g_cfg.ui.exeNotFound, vars, 1);
        MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleLaunchError.c_str(), MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring gameDir = exePath;
    size_t lastSep = gameDir.find_last_of(L"\\/");
    if (lastSep != std::wstring::npos) gameDir = gameDir.substr(0, lastSep);

    std::wstring cmdLine = L"\"" + exePath + L"\"";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(0);
    std::vector<wchar_t> dirBuf(gameDir.begin(), gameDir.end());
    dirBuf.push_back(0);

    if (!CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE,
                         CREATE_NO_WINDOW, NULL, dirBuf.data(), &si, &pi)) {
        DWORD err = GetLastError();
        wchar_t errBuf[32];
        swprintf(errBuf, 32, L"0x%08X", err);
        ExpandVar vars[] = { { L"code", std::wstring(errBuf) } };
        std::wstring msg = expand(g_cfg.ui.launchFailed, vars, 1);
        MessageBoxW(NULL, msg.c_str(), g_cfg.ui.titleLaunchError.c_str(), MB_OK | MB_ICONERROR);
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// ============================================================
//  Main Entry Point
// ============================================================

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {

    ensureConfigFile();
    loadConfig(getExeDir() + L"\\launcher.ini");

    // === PHASE -1: Single Instance Guard ===
    if (!acquireSingleInstance())
        ExitProcess(0);

    // === PHASE 0: Environment Auto-Repair ===

    if (g_cfg.autoInstallVc) {
        if (!checkAndInstallVC())
            ExitProcess(1);
    }

    std::wstring dxErr;
    if (!checkDX11(dxErr))
        ExitProcess(1);

    // === PHASE 1: Resource Checks (Collect all, show once) ===

    std::wstring combinedWarn;
    std::wstring titleLine;

    std::wstring ramWarn;
    if (checkRAM(ramWarn)) {
        if (!titleLine.empty()) titleLine += L"，";
        titleLine += g_cfg.ui.warnLabelRam;
        combinedWarn += ramWarn + L"\n\n";
    }

    std::wstring vramWarn;
    if (checkVRAM(vramWarn)) {
        if (!titleLine.empty()) titleLine += L"，";
        titleLine += g_cfg.ui.warnLabelVram;
        combinedWarn += vramWarn + L"\n\n";
    }

    std::wstring confWarn;
    if (checkConflicts(confWarn)) {
        if (!titleLine.empty()) titleLine += L"，";
        titleLine += g_cfg.ui.warnLabelConflict;
        combinedWarn += confWarn + L"\n\n";
    }

    if (!combinedWarn.empty()) {
        ExpandVar vars[] = {
            { L"title", titleLine },
            { L"details", combinedWarn }
        };
        std::wstring full = expand(g_cfg.ui.combinedWarn, vars, 2);
        int ret = MessageBoxW(NULL, full.c_str(), g_cfg.ui.titleWarning.c_str(), MB_YESNO | MB_ICONWARNING);
        if (ret != IDYES)
            ExitProcess(0);
    }

    // === PHASE 2: Launch Game ===
    launchGame();

    ExitProcess(0);
}
