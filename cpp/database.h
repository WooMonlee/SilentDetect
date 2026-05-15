#ifndef DATABASE_H
#define DATABASE_H

#include <windows.h>
#include <cstring>
#include <cstdio>

// ============================================================
// Data structures
// ============================================================

struct SilentParam {
    const char* sw;
    const char* desc;
    bool primary;
};

struct MagicBytes {
    const BYTE* data;
    int len;
};

struct InstallerInfo {
    const char* type;
    const char* fullName;
    const char* cmdTemplate;
    const SilentParam* params;
    int paramCount;
    const char** strSigs;
    int strSigCount;
    const MagicBytes* magics;
    int magicCount;
    const char** secNames;
    int secNameCount;
};

// ============================================================
// UTF-8 to Wide string helper (for Win32 API display)
// ============================================================

inline int Utf8ToWide(const char* utf8, wchar_t* wide, int wideLen) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideLen);
}

// ============================================================
// #1 NSIS
// ============================================================

static const SilentParam pNSIS[] = {
    {"/S",        "静默安装 (必需，大写)", true},
    {"/D=<path>", "指定安装目录 (必须为最后一个参数)"},
    {"/NCRC",     "跳过CRC完整性校验"},
    {"/NOCD",     "禁用当前目录更改"}
};
static const char* sNSIS[] = {"NullsoftInst", "NSIS Error", "!@Install@!UTF-8!", "NSIS_INSTALLDIR"};
static const BYTE mNSIS_d[] = {0xEF, 0xBE, 0xAD, 0xDE};
static const MagicBytes mNSIS[] = {{mNSIS_d, 4}};
static const char* secNSIS[] = {".ndata", ".nsis"};

// ============================================================
// #2 Inno Setup
// ============================================================

static const SilentParam pInno[] = {
    {"/VERYSILENT",          "完全静默 (推荐)", true},
    {"/SILENT",              "静默模式 (显示进度条)"},
    {"/SUPPRESSMSGBOXES",    "抑制所有消息框"},
    {"/SP-",                 "禁用初始安装提示"},
    {"/NORESTART",           "禁止重启提示"},
    {"/NOCANCEL",            "禁止取消安装"},
    {"/DIR=\"x:\\path\"",    "指定安装目录"},
    {"/GROUP=\"folder\"",    "设置开始菜单文件夹"},
    {"/NOICONS",             "不创建开始菜单图标"},
    {"/LOADINF=\"file\"",    "从INF文件加载设置"},
    {"/SAVEINF=\"file\"",    "保存设置到INF文件"},
    {"/LANG=language",       "设置安装语言"},
    {"/COMPONENTS=\"c1,c2\"","选择安装组件"},
    {"/TASKS=\"t1,t2\"",     "选择执行任务"},
    {"/PASSWORD=pwd",        "提供加密安装包密码"},
    {"/LOG=\"file\"",        "启用安装日志"}
};
static const char* sInno[] = {"Inno Setup Setup Data", "JR.Inno.Setup", "Inno Setup", "InnoSetup:"};
static const BYTE mInno_d[] = {0x72, 0x44, 0x6C, 0x50, 0x74, 0x53};
static const MagicBytes mInno[] = {{mInno_d, 6}};
static const char* secInno[] = {".itext"};

// ============================================================
// #3 InstallShield
// ============================================================

static const SilentParam pIS[] = {
    {"/s",                "静默模式 (必需，小写)", true},
    {"/v\"/qn\"",         "传递静默参数给MSI引擎"},
    {"/v\"/qb\"",         "基本UI (仅进度条)"},
    {"/r",                "录制响应文件 (.iss)"},
    {"/r /f1\"path.iss\"","录制响应文件到指定路径"},
    {"/f1\"path.iss\"",   "使用指定响应文件"},
    {"/f2\"path.log\"",   "指定日志文件路径"},
    {"/m\"file.iss\"",    "维护模式的ISS文件"},
    {"/uninst",           "卸载模式"}
};
static const char* sIS[] = {"InstallShield", "InstallShield Wizard", "ISSETUP.DLL", "_setup.dll", "InstallShield Setup"};
static const BYTE mIS_d0[] = {0x49, 0x53, 0x63, 0x34};
static const BYTE mIS_d1[] = {0x49, 0x53, 0x63, 0x35};
static const BYTE mIS_d2[] = {0x49, 0x53, 0x63, 0x36};
static const MagicBytes mIS[] = {{mIS_d0, 4}, {mIS_d1, 4}, {mIS_d2, 4}};
static const char* secIS[] = {".isg", ".ISG"};

// ============================================================
// #4 MSI
// ============================================================

static const SilentParam pMSI[] = {
    {"msiexec /i",                "安装 (必需前缀)", true},
    {"/qn",                       "完全静默，无UI"},
    {"/qb",                       "基本UI (仅进度条)"},
    {"/qb!",                      "基本UI，无取消按钮"},
    {"/passive",                  "被动模式 (进度条，无交互)"},
    {"/quiet",                    "静默模式 (等同/qn)"},
    {"/norestart",                "禁止重启"},
    {"/forcerestart",             "安装后强制重启"},
    {"/l*v \"file.log\"",         "详细日志记录"},
    {"ALLUSERS=1",                "所有用户安装 (计算机级)"},
    {"REBOOT=ReallySuppress",     "抑制重启"},
    {"INSTALLDIR=\"C:\\Path\"",   "指定安装目录"},
    {"ADDLOCAL=ALL",              "安装所有功能"},
    {"TRANSFORMS=\"custom.mst\"", "应用转换文件"}
};
static const char* sMSI[] = {"Windows Installer", "msiexec"};
static const BYTE mMSI_d[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
static const MagicBytes mMSI[] = {{mMSI_d, 8}};

// ============================================================
// #5 WiX Burn
// ============================================================

static const SilentParam pWiXBurn[] = {
    {"/quiet",              "完全静默 (必需)", true},
    {"/passive",            "被动模式 (仅进度条)"},
    {"/norestart",          "禁止重启"},
    {"/log \"file.log\"",   "启用日志"},
    {"/layout \"path\"",    "下载包到指定路径"},
    {"/uninstall",          "卸载"},
    {"/repair",             "修复安装"}
};
static const char* sWiXBurn[] = {"BootstrapperCore.dll", "WixBundle", "WixStandardBootstrapperApplication", "WixStdBA"};

// ============================================================
// #6 Advanced Installer
// ============================================================

static const SilentParam pAI[] = {
    {"/q",                      "静默模式 (必需)", true},
    {"/quiet",                  "静默模式 (备用)"},
    {"/qb",                     "基本UI (进度条)"},
    {"/qr",                     "精简UI"},
    {"/passive",                "被动模式"},
    {"/norestart",              "禁止重启"},
    {"/L*V \"file.log\"",       "详细日志"},
    {"REBOOT=ReallySuppress",   "抑制重启"},
    {"INSTALLDIR=\"C:\\Path\"", "指定安装目录"}
};
static const char* sAI[] = {"AdvancedInstaller", "Advanced Installer", "Caphyon", "AI_SETUP", "advancedinstaller.com"};

// ============================================================
// #7 Setup Factory
// ============================================================

static const SilentParam pSF[] = {
    {"/S",        "静默模式 (必需，大写)", true},
    {"/s",        "静默模式 (小写)"},
    {"/D=<path>", "指定安装目录"},
    {"/NOCANCEL", "禁用取消按钮"}
};
static const char* sSF[] = {"SetupFactory", "Setup Factory", "Indigo Rose", "AFData", "SUF_", "irsetup"};

// ============================================================
// #8 Smart Install Maker
// ============================================================

static const SilentParam pSIM[] = {
    {"/S",            "静默模式", true},
    {"/silent",       "静默模式 (备用)"},
    {"/SILENT",       "静默模式 (大写)"},
    {"/VERYSILENT",   "完全静默"},
    {"/DIR=\"C:\\Path\"", "指定安装目录"},
    {"/NORESTART",    "禁止重启"}
};
static const char* sSIM[] = {"Smart Install Maker", "InstallBuilders", "smartinstallmaker"};

// ============================================================
// #9 Ghost Installer
// ============================================================

static const SilentParam pGhost[] = {
    {"/S",                "静默模式", true},
    {"/verysilent",       "完全静默"},
    {"/sp-",              "禁用初始提示"},
    {"/norestart",        "禁止重启"},
    {"/dir=\"C:\\Path\"", "指定安装目录"}
};
static const char* sGhost[] = {"GHOST", "Ghost Installer", "GHOST Installer Studio", "Extreme Internet", "ginstall", "ghostinstall"};

// ============================================================
// #10 CreateInstall
// ============================================================

static const SilentParam pCI[] = {
    {"/S",        "静默模式 (必需)", true},
    {"/s",        "静默模式 (小写)"},
    {"/D=<path>", "指定安装目录"},
    {"/NOCANCEL", "禁用取消按钮"},
    {"/NCRC",     "跳过CRC校验"}
};
static const char* sCI[] = {"CreateInstall", "createinstall.com"};

// ============================================================
// #11 Actual Installer
// ============================================================

static const SilentParam pActual[] = {
    {"/S",        "静默模式 (必需，大写)", true},
    {"/SILENT",   "静默模式 (备用)"},
    {"/D=<path>", "指定安装目录 (必须为最后一个参数)"},
    {"/NOCANCEL", "禁用取消按钮"}
};
static const char* sActual[] = {"Actual Installer", "Softeza", "actualinstaller"};

// ============================================================
// #12 SetupBuilder
// ============================================================

static const SilentParam pSB[] = {
    {"/S",                   "静默模式", true},
    {"/Q",                   "安静模式"},
    {"/SILENT",              "静默模式"},
    {"/VERYSILENT",          "完全静默"},
    {"/SP-",                 "禁用初始提示"},
    {"/NORESTART",           "禁止重启"},
    {"/SUPPRESSMSGBOXES",    "抑制消息框"},
    {"/DIR=\"x:\\path\"",    "指定安装目录"},
    {"/NOICONS",             "不创建图标"},
    {"/LOG=\"file.log\"",    "安装日志"},
    {"/SAVETOSILENT",        "生成静默安装配置文件"}
};
static const char* sSB[] = {"SetupBuilder", "Lindersoft", "sbsetup", "setupbuilder"};

// ============================================================
// #13 InstallAware
// ============================================================

static const SilentParam pIA[] = {
    {"/s",                          "静默模式 (必需)", true},
    {"/v\"/qn\"",                   "传递MSI静默参数"},
    {"/v\"/l*v file.log\"",         "详细日志"},
    {"/v\"REBOOT=ReallySuppress\"", "抑制重启"},
    {"/x",                          "静默卸载"}
};
static const char* sIA[] = {"InstallAware", "installaware.com", "MsiAware"};

// ============================================================
// #14 Wise Installer
// ============================================================

static const SilentParam pWise[] = {
    {"/s",           "静默模式", true},
    {"/S",           "静默模式 (大写)"},
    {"/q",           "安静模式"},
    {"/qn",          "安静，无UI"},
    {"/s /v\"/qn\"", "传递MSI静默参数"},
    {"/x",           "卸载"}
};
static const char* sWise[] = {"WiseMain", "WiseScript", "__WISE__", "Wise Solutions", "Wise Installation"};

// ============================================================
// #15 RAR SFX
// ============================================================

static const SilentParam pRAR[] = {
    {"-s",          "静默模式 (抑制所有提示)", true},
    {"-s1",         "仅隐藏解压对话框"},
    {"-s2",         "隐藏所有对话框"},
    {"-d<path>",    "指定目标目录"},
    {"-o+",         "覆盖已有文件"},
    {"-o-",         "不覆盖已有文件"},
    {"-p<password>", "提供加密密码"},
    {"-ep1",        "排除基础目录"}
};
static const char* sRAR[] = {"Rar!", "WinRAR", "RAR SFX"};
static const BYTE mRAR_d[] = {0x52, 0x61, 0x72, 0x21};
static const MagicBytes mRAR[] = {{mRAR_d, 4}};

// ============================================================
// #16 7-Zip SFX
// ============================================================

static const SilentParam p7z[] = {
    {"-y",          "对所有查询假设为是", true},
    {"-aoa",        "覆盖所有已有文件"},
    {"-aos",        "跳过已有文件"},
    {"-aou",        "自动重命名解压文件"},
    {"-o<path>",    "指定输出目录"},
    {"-bso0",       "抑制标准输出"},
    {"-bsp0",       "抑制进度输出"},
    {"-p<password>", "提供密码"}
};
static const char* s7z[] = {"7-Zip", "7zSFX", "7zS2", "Igor Pavlov"};
static const BYTE m7z_d[] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};
static const MagicBytes m7z[] = {{m7z_d, 6}};

// ============================================================
// #17 Clickteam Install Creator
// ============================================================

static const SilentParam pCT[] = {
    {"/S",            "静默安装", true},
    {"/SILENT",       "静默模式 (备用)"},
    {"/D=<path>",     "指定安装目录"},
    {"/NORESTART",    "禁止重启"},
    {"/VERYSILENT",   "完全静默"},
    {"/SUPPRESSMSGBOXES", "抑制消息框"}
};
static const char* sCT[] = {"Clickteam", "Install Creator", "InstallCreator"};

// ============================================================
// #18 Paquet Builder
// ============================================================

static const SilentParam pPB[] = {
    {"/SILENT",     "静默模式", true},
    {"/VERYSILENT", "完全静默"},
    {"/S",          "静默模式 (备用)"},
    {"/q",          "安静模式"},
    {"/quiet",      "安静模式"}
};
static const char* sPB[] = {"Paquet Builder", "GDG Soft", "GDGSoft", "paquetbuilder"};

// ============================================================
// #19 AutoPlay Media Studio
// ============================================================

static const SilentParam pAP[] = {
    {"/S",                        "静默安装", true},
    {"/S /v\"/qn\"",              "静默 + MSI /qn"},
    {"/S /v\"ALLUSERS=1 /qn\"",   "所有用户静默安装"}
};
static const char* sAP[] = {"AutoPlay Media Studio", "Indigo Rose", "IR_SETUP"};

// ============================================================
// #20 MSI Wrapped EXE
// ============================================================

static const SilentParam pMSIWrap[] = {
    {"/qn /norestart",               "MSI静默参数 (尝试)", true},
    {"/quiet /norestart",            "静默参数 (备用)"},
    {"/s /v\"/qn\"",                 "InstallShield封装格式"},
    {"/q",                           "Advanced Installer格式"},
    {"msiexec /i extracted.msi /qn", "先解压MSI再静默安装"}
};
static const char* sMSIWrap[] = {"msiexec", "Windows Installer"};

// ============================================================
// #21 IExpress / WExtract
// ============================================================

static const SilentParam pIExpress[] = {
    {"/Q",              "静默模式 (抑制所有UI)", true},
    {"/Q:u",            "用户级静默 (仅抑制用户UI)"},
    {"/Q:a",            "管理员级静默 (抑制所有UI)"},
    {"/T:<fullpath>",   "指定解压目录"},
    {"/C:<cmd>",        "指定解压后执行的命令"},
    {"/C:\"cmd args\"",  "带参数的解压后命令"},
    {"/R:n",            "安装后不重启"},
    {"/R:s",            "安装后强制重启"},
    {"/D:<path>",       "指定默认解压路径"}
};
static const char* sIExpress[] = {"WExtract", "IExpress", "wextract.exe", "Microsoft Self-Extractor", "CABINET"};

// ============================================================
// #22 Squirrel (Electron Apps)
// ============================================================

static const SilentParam pSquirrel[] = {
    {"--silent",                              "静默安装 (必需)", true},
    {"--force-run",                           "安装后立即启动应用"},
    {"--createShortcut",                      "创建桌面快捷方式"},
    {"--no-createShortcut",                   "不创建快捷方式"},
    {"--shortcut-locations=desktop,startmenu", "指定快捷方式位置"},
    {"--update-only",                         "仅更新已安装版本"}
};
static const char* sSquirrel[] = {"Squirrel", "SquirrelSetup", "SquirrelUpdate", "nupkg", "NuGet", "electron-builder"};

// ============================================================
// #23 BitRock InstallBuilder
// ============================================================

static const SilentParam pBitRock[] = {
    {"--mode unattended",          "完全静默 (必需)", true},
    {"--mode text",                "文本模式 (命令行交互)"},
    {"--unattendedmodeui none",    "无UI静默"},
    {"--unattendedmodeui minimal", "最小化UI"},
    {"--prefix \"C:\\Path\"",      "指定安装目录"},
    {"--installer-language en",    "设置安装语言"},
    {"--optionfile \"file.txt\"",   "使用选项文件"},
    {"--debuglevel 4",             "调试级别 (日志详细度)"},
    {"--enable-components comp1",  "启用指定组件"},
    {"--disable-components comp1", "禁用指定组件"}
};
static const char* sBitRock[] = {"BitRock InstallBuilder", "installbuilder", "BitRock", "VMware InstallBuilder"};

// ============================================================
// #24 InstallScript
// ============================================================

static const SilentParam pISScript[] = {
    {"/s",                    "静默模式 (必需)", true},
    {"/f1\"path\\setup.iss\"", "指定ISS响应文件路径"},
    {"/f2\"path\\setup.log\"", "指定日志文件路径"},
    {"/r",                    "录制模式 (生成ISS响应文件)"},
    {"/r /f1\"path\\setup.iss\"", "录制到指定路径"},
    {"/z\"param=value\"",     "传递自定义参数"},
    {"/x",                    "静默卸载"},
    {"/m\"productname\"",     "维护模式"},
    {"/uninst",               "启动卸载程序"}
};
static const char* sISScript[] = {"InstallScript", "setup.iss", "InstallShield Script", "IScriptModule", "isrt.dll", "isbew.exe"};

// ============================================================
// #25 WinZip Self-Extractor
// ============================================================

static const SilentParam pWinZip[] = {
    {"-auto",       "自动解压 (无对话框)", true},
    {"-o",          "解压到指定目录 (后跟路径)"},
    {"-y",          "覆盖已有文件时不提示"},
    {"-inst",       "解压后运行安装命令"},
    {"-j",          "解压时不包含路径信息"},
    {"-d<path>",    "指定目标目录"},
    {"-np",         "不显示进度窗口"}
};
static const char* sWinZip[] = {"WinZip", "WinZip Self-Extractor", "PKWARE", "WinZipSFX", "self-extractor"};

// ============================================================
// #26 WiX MSI
// ============================================================

static const SilentParam pWiXMSI[] = {
    {"msiexec /i",                "安装 (必需前缀)", true},
    {"/qn",                       "完全静默，无UI"},
    {"/qb",                       "基本UI (仅进度条)"},
    {"/passive",                  "被动模式 (进度条无交互)"},
    {"/norestart",                "禁止重启"},
    {"/l*v \"file.log\"",         "详细日志记录"},
    {"ALLUSERS=1",                "所有用户安装"},
    {"REBOOT=ReallySuppress",     "抑制重启"},
    {"ADDLOCAL=ALL",              "安装所有功能"},
    {"MSIFASTINSTALL=7",          "快速安装优化"}
};
static const char* sWiXMSI[] = {"WixUI", "WiX", "wixlib", "WiX Toolset", "MSI498", "WindowsInstaller"};
static const BYTE mWiXMSI_d[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
static const MagicBytes mWiXMSI[] = {{mWiXMSI_d, 8}};

// ============================================================
// #27 APPX
// ============================================================

static const SilentParam pAPPX[] = {
    {"Add-AppxPackage -Path",              "安装应用包 (必需)", true},
    {"-DependencyPath \"dep.appx\"",       "指定依赖包路径"},
    {"-ForceApplicationShutdown",          "强制关闭运行中的应用"},
    {"-ForceUpdateFromAnyVersion",         "强制覆盖任意版本"},
    {"-Update",                            "更新已安装的应用"},
    {"-AllowUnsigned",                     "允许安装未签名包"},
    {"-ExternalLocation \"path\"",         "指定外部位置"}
};
static const char* sAPPX[] = {"AppxManifest", "AppxBlockMap", "AppxSignature", "windows.com/appx"};
static const BYTE mPK_d[] = {0x50, 0x4B, 0x03, 0x04};
static const MagicBytes mAPPX[] = {{mPK_d, 4}};

// ============================================================
// #28 MSIX
// ============================================================

static const SilentParam pMSIX[] = {
    {"Add-AppxPackage -Path",              "安装应用包 (必需)", true},
    {"-DependencyPath \"dep.msix\"",       "指定依赖包路径"},
    {"-ForceApplicationShutdown",          "强制关闭运行中的应用"},
    {"-ForceUpdateFromAnyVersion",         "强制覆盖任意版本"},
    {"-Update",                            "更新已安装的应用"},
    {"-AllowUnsigned",                     "允许安装未签名包"},
    {"-ExternalLocation \"path\"",         "指定外部位置"},
    {"-VolumePath \"path\"",               "指定安装卷路径"}
};
static const char* sMSIX[] = {"AppxManifest", "MSIX", "msix", "windows.com/msix"};
static const MagicBytes mMSIX[] = {{mPK_d, 4}};

// ============================================================
// #29 MSIXBundle
// ============================================================

static const SilentParam pMSIXB[] = {
    {"Add-AppxPackage -Path",              "安装捆绑包 (必需)", true},
    {"-DependencyPath \"dep.msix\"",       "指定依赖包路径"},
    {"-ForceApplicationShutdown",          "强制关闭运行中的应用"},
    {"-ForceUpdateFromAnyVersion",         "强制覆盖任意版本"},
    {"-Update",                            "更新已安装的应用"},
    {"-AllowUnsigned",                     "允许安装未签名包"}
};
static const char* sMSIXB[] = {"AppxBundleManifest", "MSIXBundle", "msixbundle", "windows.com/appxbundle"};
static const MagicBytes mMSIXB[] = {{mPK_d, 4}};

// ============================================================
// Master database
// ============================================================

// Macros: _S = has string sigs, _M = has magic, _SM = has both
#define E_S(t, fn, ct, p, ss) \
    {t, fn, ct, p, sizeof(p)/sizeof(p[0]), (const char**)ss, sizeof(ss)/sizeof(ss[0]), nullptr, 0, nullptr, 0}
#define E_SM(t, fn, ct, p, ss, m) \
    {t, fn, ct, p, sizeof(p)/sizeof(p[0]), (const char**)ss, sizeof(ss)/sizeof(ss[0]), m, sizeof(m)/sizeof(m[0]), nullptr, 0}
#define E_SMS(t, fn, ct, p, ss, m, sn) \
    {t, fn, ct, p, sizeof(p)/sizeof(p[0]), (const char**)ss, sizeof(ss)/sizeof(ss[0]), m, sizeof(m)/sizeof(m[0]), (const char**)sn, sizeof(sn)/sizeof(sn[0])}
#define E_M(t, fn, ct, p, m) \
    {t, fn, ct, p, sizeof(p)/sizeof(p[0]), nullptr, 0, m, sizeof(m)/sizeof(m[0]), nullptr, 0}

static const InstallerInfo g_database[] = {
    // type,                     full name,                                  cmd template,                     params,    strSigs,   magic,   secNames
    E_SMS("NSIS",                "Nullsoft Scriptable Install System",        "\"{file}\" /S",                       pNSIS,      sNSIS,      mNSIS,    secNSIS),
    E_SMS("InnoSetup",           "Inno Setup",                                "\"{file}\" /VERYSILENT /SP-",         pInno,      sInno,      mInno,    secInno),
    E_SMS("InstallShield",       "InstallShield",                             "\"{file}\" /s /v\"/qn\"",             pIS,        sIS,        mIS,      secIS),
    E_M  ("MSI",                 "Windows Installer (MSI)",                   "msiexec /i \"{file}\" /qn /norestart",pMSI,       mMSI),
    E_S  ("WiXBurn",             "WiX Burn Bundle",                           "\"{file}\" /quiet /norestart",        pWiXBurn,   sWiXBurn),
    E_S  ("AdvancedInstaller",   "Advanced Installer (Caphyon)",              "\"{file}\" /q",                       pAI,        sAI),
    E_S  ("SetupFactory",        "Setup Factory (Indigo Rose)",               "\"{file}\" /S",                       pSF,        sSF),
    E_S  ("SmartInstallMaker",   "Smart Install Maker",                       "\"{file}\" /S",                       pSIM,       sSIM),
    E_S  ("GhostInstaller",      "Ghost Installer",                           "\"{file}\" /S",                       pGhost,     sGhost),
    E_S  ("CreateInstall",       "CreateInstall (Gentee)",                    "\"{file}\" /S",                       pCI,        sCI),
    E_S  ("ActualInstaller",     "Actual Installer (Softeza)",                "\"{file}\" /S",                       pActual,    sActual),
    E_S  ("SetupBuilder",        "SetupBuilder (Lindersoft)",                 "\"{file}\" /S",                       pSB,        sSB),
    E_S  ("InstallAware",        "InstallAware",                              "\"{file}\" /s /v\"/qn\"",             pIA,        sIA),
    E_S  ("WiseInstaller",       "Wise Installer",                            "\"{file}\" /s",                       pWise,      sWise),
    E_SM ("RARSFX",              "RAR Self-Extracting Archive",               "\"{file}\" -s -o+",                   pRAR,       sRAR,       mRAR),
    E_SM ("7ZipSFX",             "7-Zip Self-Extracting Archive",             "\"{file}\" -y -aoa",                  p7z,        s7z,        m7z),
    E_S  ("ClickteamIC",         "Clickteam Install Creator",                 "\"{file}\" /S",                       pCT,        sCT),
    E_S  ("PaquetBuilder",       "Paquet Builder (GDG Soft)",                 "\"{file}\" /SILENT",                  pPB,        sPB),
    E_S  ("AutoPlayMS",          "AutoPlay Media Studio (Indigo Rose)",       "\"{file}\" /S",                       pAP,        sAP),
    E_S  ("MSIWrappedEXE",       "Generic MSI-Wrapped EXE",                   "\"{file}\" /qn /norestart",           pMSIWrap,   sMSIWrap),
    E_S  ("IExpress",            "IExpress / WExtract",                       "\"{file}\" /Q",                       pIExpress,  sIExpress),
    E_S  ("Squirrel",            "Squirrel.Windows (Electron Apps)",          "\"{file}\" --silent",                 pSquirrel,  sSquirrel),
    E_S  ("BitRockInstallBuilder","BitRock InstallBuilder",                   "\"{file}\" --mode unattended",        pBitRock,   sBitRock),
    E_S  ("InstallScript",       "InstallShield InstallScript",               "\"{file}\" /s /f1\"setup.iss\"",      pISScript,  sISScript),
    E_S  ("WinZipSFX",           "WinZip Self-Extractor",                     "\"{file}\" -auto -o \"C:\\Path\"",    pWinZip,    sWinZip),
    E_SM ("WiXMSI",              "WiX Toolset MSI Package",                   "msiexec /i \"{file}\" /qn /norestart",pWiXMSI,    sWiXMSI,    mWiXMSI),
    E_SM ("APPX",                "APPX (Windows App Package)",                "powershell Add-AppxPackage -Path '{file}'", pAPPX, sAPPX,  mAPPX),
    E_SM ("MSIX",                "MSIX (Modern Windows App Package)",         "powershell Add-AppxPackage -Path '{file}'", pMSIX, sMSIX,  mMSIX),
    E_SM ("MSIXBundle",          "MSIXBundle (App Bundle Package)",           "powershell Add-AppxPackage -Path '{file}'", pMSIXB,sMSIXB, mMSIXB),
};

static const int g_dbCount = sizeof(g_database) / sizeof(g_database[0]);

// ============================================================
// Query functions
// ============================================================

inline const InstallerInfo* DB_FindByType(const char* type) {
    for (int i = 0; i < g_dbCount; i++)
        if (_stricmp(g_database[i].type, type) == 0)
            return &g_database[i];
    return nullptr;
}

inline int DB_GetCount() { return g_dbCount; }

#endif // DATABASE_H
