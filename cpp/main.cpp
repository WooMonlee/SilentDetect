#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <cstdio>
#include <thread>
#include <future>  // 修复：用于后台线程安全等待
#include "resource.h"
#include "database.h"
#include "scanner.h"
#include "batgen.h"
#include "batchrun.h"

#pragma comment(lib, "ole32.lib")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================
// Globals
// ============================================================

static HINSTANCE g_hInst;
static HWND g_hWnd;
static bool g_toolsExtractedToTemp = false; // 修复：替代main.cpp中重复的g_toolsPath，标记是否解压到临时目录
static std::future<void> g_extractFuture; // 修复：后台解压线程future，用于安全等待

// ============================================================
// Debug log to file
// ============================================================

// 日志开关：定义DEBUG_LOG宏时输出debug_log.txt，否则日志函数为空操作
#ifdef DEBUG_LOG
static std::wstring g_logPath;

static void InitLog() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeDir = exeDir.substr(0, lastSlash);
    g_logPath = exeDir + L"\\debug_log.txt";

    FILE* f = _wfopen(g_logPath.c_str(), L"w");
    if (f) {
        fprintf(f, "=== Debug Log ===\n");
        fclose(f);
    }
}

static void Log(const char* fmt, ...) {
    FILE* f = _wfopen(g_logPath.c_str(), L"a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

static void LogW(const wchar_t* fmt, ...) {
    FILE* f = _wfopen(g_logPath.c_str(), L"a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfwprintf(f, fmt, args);
    va_end(args);
    fwprintf(f, L"\n");
    fclose(f);
}
#else
static inline void InitLog() {}
static inline void Log(const char*, ...) {}
static inline void LogW(const wchar_t*, ...) {}
#endif

// ============================================================
// Extract embedded Tools.zip to temp directory
// ============================================================

static bool ExtractToolsZip() {
    Log("=== ExtractToolsZip START ===");

    // Find resource (RT_RCDATA needs cast for MinGW)
    HRSRC hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(IDR_TOOLS_ZIP), (LPCWSTR)RT_RCDATA);
    Log("FindResourceW: %s", hRes ? "OK" : "FAILED");
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(g_hInst, hRes);
    Log("LoadResource: %s", hData ? "OK" : "FAILED");
    if (!hData) return false;

    DWORD dataSize = SizeofResource(g_hInst, hRes);
    void* data = LockResource(hData);
    Log("Resource size: %lu bytes, data ptr: %p", dataSize, data);
    if (!data || dataSize == 0) return false;

    // Get temp directory
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    LogW(L"Temp dir: %s", tempDir);

    // Create unique directory for our Tools
    std::wstring toolsDir = std::wstring(tempDir) + L"SilentParamQuery_Tools";
    CreateDirectoryW(toolsDir.c_str(), NULL);
    LogW(L"Tools dir: %s", toolsDir.c_str());

    // Check if already extracted (by checking for diec.exe)
    std::wstring diecCheck = toolsDir + L"\\Tools\\diec.exe";
    if (GetFileAttributesW(diecCheck.c_str()) != INVALID_FILE_ATTRIBUTES) {
        SetToolsPath(toolsDir + L"\\Tools"); // 修复：直接同步到scanner.cpp，消除main.cpp重复g_toolsPath
        g_toolsExtractedToTemp = true;
        Log("Already extracted, found diec.exe");
        return true;
    }

    // Write ZIP to temp file
    std::wstring zipPath = toolsDir + L"\\tools.zip";
    FILE* f = _wfopen(zipPath.c_str(), L"wb");
    Log("Writing zip to: %s, fopen: %s", "tools.zip", f ? "OK" : "FAILED");
    if (!f) return false;
    fwrite(data, 1, dataSize, f);
    fclose(f);
    Log("Zip written successfully");

    // Extract using PowerShell (more reliable than tar for zip files)
    std::wstring extractDir = toolsDir + L"\\extract";
    CreateDirectoryW(extractDir.c_str(), NULL);

    bool extracted = false;

    // Use PowerShell to extract
    // 修复：PowerShell命令注入 — 转义路径中的单引号（PowerShell中用 '' 表示一个字面单引号）
    std::wstring escapedZip = zipPath, escapedDir = extractDir;
    for (std::wstring* s : {&escapedZip, &escapedDir}) {
        size_t p = 0;
        while ((p = s->find(L'\'', p)) != std::wstring::npos) {
            s->insert(p, L"'");
            p += 2;
        }
    }
    std::wstring cmd = L"powershell -NoProfile -Command \"Expand-Archive -Path '"
        + escapedZip + L"' -DestinationPath '" + escapedDir + L"' -Force\"";
    LogW(L"Running command: %s", cmd.c_str());

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        Log("PowerShell exit code: %lu", exitCode);
        if (exitCode == 0) extracted = true;
    } else {
        Log("CreateProcessW FAILED");
    }

    if (!extracted) {
        Log("Extraction failed!");
        return false;
    }

    // Check extraction result - try multiple possible paths
    std::wstring toolsPath = extractDir + L"\\Tools";
    DWORD attr = GetFileAttributesW(toolsPath.c_str());
    LogW(L"Checking path: %s, exists: %s", toolsPath.c_str(),
        (attr != INVALID_FILE_ATTRIBUTES) ? "YES" : "NO");

    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetToolsPath(toolsPath); // 修复：同步到scanner.cpp
        g_toolsExtractedToTemp = true;
        DeleteFileW(zipPath.c_str());
        LogW(L"Tools found at: %s", toolsPath.c_str());
        return true;
    }

    // Try alternate path (directly in extract dir)
    toolsPath = extractDir;
    attr = GetFileAttributesW((toolsPath + L"\\diec.exe").c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        SetToolsPath(toolsPath); // 修复：同步到scanner.cpp
        g_toolsExtractedToTemp = true;
        DeleteFileW(zipPath.c_str());
        return true;
    }

    return false;
}

// Check if Tools directory exists in exe's directory (for development/portable mode)
static bool FindToolsInExeDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeDir = exeDir.substr(0, lastSlash);

    std::wstring toolsPath = exeDir + L"\\Tools";
    DWORD attr = GetFileAttributesW(toolsPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        // Check if diec.exe exists
        std::wstring diecPath = toolsPath + L"\\diec.exe";
        attr = GetFileAttributesW(diecPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            SetToolsPath(toolsPath); // 修复：同步到scanner.cpp
            return true;
        }
    }
    return false;
}

static HWND g_hGrpDrop, g_hLblDrop, g_hBtnBrowse, g_hLblFilePath;
static HWND g_hGrpResult;
static HWND g_hLblTypeCap, g_hLblType, g_hLblVerCap, g_hLblVer;
static HWND g_hLblDetCap, g_hLblDet, g_hLblConfCap, g_hLblConf;
static HWND g_hGrpParams, g_hLstParams;
static HWND g_hGrpCmd, g_hTxtCmd, g_hBtnCopy;
static HWND g_hGrpBatch, g_hChkLog, g_hChkErr, g_hChkUac, g_hChkComment, g_hBtnGenBat;
static HWND g_hGrpRun, g_hBtnRunExes, g_hBtnRunAll;
static HWND g_hProgress;
static HWND g_hStatusBar;

static HFONT g_hFontNormal, g_hFontBold, g_hFontMono;
static HBRUSH g_hBrushDrop;
static HICON g_hIcon;

static ScanResult g_currentResult;
static bool g_hasResult = false;

// ============================================================
// Command-line argument parsing
// ============================================================

struct CliArgs {
    std::wstring outFile;       // /out:file or -out:file
    bool runExes = false;       // /run or -run
    bool runAll = false;        // /all or -all
    bool copyCmd = false;       // /copy or -copy
    std::wstring inputFile;     // file to scan (no prefix)
};

static CliArgs ParseCommandLine(LPWSTR lpCmdLine) {
    CliArgs args;
    std::wstring cmdLine(lpCmdLine);

    size_t i = 0;
    while (i < cmdLine.size()) {
        while (i < cmdLine.size() && cmdLine[i] == L' ') i++;
        if (i >= cmdLine.size()) break;

        std::wstring token;
        if (cmdLine[i] == L'"') {
            i++;
            while (i < cmdLine.size() && cmdLine[i] != L'"') {
                token += cmdLine[i];
                i++;
            }
            if (i < cmdLine.size()) i++;
        } else {
            while (i < cmdLine.size() && cmdLine[i] != L' ') {
                token += cmdLine[i];
                i++;
            }
        }

        if (token.empty()) continue;

        if (token[0] == L'-' || token[0] == L'/') {
            std::wstring sw = token.substr(1);
            for (auto& c : sw) {
                if (c == L'\xFF1A') c = L':'; // fullwidth colon → ASCII
            }

            if (sw.find(L"out:") == 0) {
                args.outFile = sw.substr(4); // after "out:"
                if (args.outFile.size() >= 2 && args.outFile.front() == L'"' && args.outFile.back() == L'"')
                    args.outFile = args.outFile.substr(1, args.outFile.size() - 2);
            }
            else if (sw == L"run")  args.runExes = true;
            else if (sw == L"all")  args.runAll = true;
            else if (sw == L"copy") args.copyCmd = true;
        } else {
            args.inputFile = token;
        }
    }

    return args;
}

// Headless mode: scan file and write results to output file
// (defined after AToW helper below)

// ============================================================
// Helpers
// ============================================================

static std::wstring AToW(const char* s) {
    if (!s || !*s) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
    return ws;
}

static std::wstring AToW(const std::string& s) {
    return AToW(s.c_str());
}

// Headless mode: scan file and write results to output file
static int HeadlessScan(const std::wstring& inputFile, const std::wstring& outFile) {
    if (!FindToolsInExeDir()) {
        if (!ExtractToolsZip()) {
            // 修复：MinGW不支持_wfopen的ccs=UTF-8扩展，改用二进制写+BOM
            FILE* f = _wfopen(outFile.c_str(), L"wb");
            if (f) {
                unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                fwrite(bom, 1, 3, f);
                fwrite("\xE9\x94\x99\xE8\xAF\xAF: \xE5\xB7\xA5\xE5\x85\xB7\xE7\xBB\x84\xE4\xBB\xB6\xE8\xA7\xA3\xE5\x8E\x8B\xE5\xA4\xB1\xE8\xB4\xA5\n", 1, 30, f);
                fclose(f);
            }
            return 1;
        }
    }
    // FindToolsInExeDir / ExtractToolsZip already call SetToolsPath internally

    DetectorEngine engine;
    ScanResult result = engine.Scan(inputFile);

    // Build output as UTF-8 string (avoid fwprintf + ccs=UTF-8 which is unsupported in MinGW)
    std::string out;
    out.reserve(4096);

    auto WToU8 = [](const std::wstring& ws) -> std::string {
        if (ws.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return "";
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr);
        return s;
    };

    out += "=== \xE5\xA4\xA7\xE5\x86\x85\xE9\x9D\x99\xE6\x8E\xA2 \xE6\x89\xAB\xE6\x8F\x8F\xE7\xBB\x93\xE6\x9E\x9C ===\r\n\r\n";
    out += "\xE6\x96\x87\xE4\xBB\xB6: " + WToU8(inputFile) + "\r\n";

    if (result.success) {
        out += "\xE5\xAE\x89\xE8\xA3\x85\xE5\x99\xA8\xE7\xB1\xBB\xE5\x9E\x8B: " + result.installerFullName + " (" + result.installerType + ")\r\n";
        out += "\xE7\x89\x88\xE6\x9C\xAC: --\r\n";
        out += "\xE6\xA3\x80\xE6\xB5\x8B\xE6\x96\xB9\xE5\xBC\x8F: " + result.detectedBy + "\r\n";
        out += "\xE7\xBD\xAE\xE4\xBF\xA1\xE5\xBA\xA6: " + WToU8(result.GetConfidenceStars()) + "\r\n";
        out += "\xE9\x9D\x99\xE9\xBB\x98\xE5\x91\xBD\xE4\xBB\xA4: " + result.silentCommand + "\r\n\r\n";
        out += "\xE9\x9D\x99\xE9\xBB\x98\xE5\xAE\x89\xE8\xA3\x85\xE5\x8F\x82\xE6\x95\xB0:\r\n";
        for (const auto* p : result.params) {
            if (!p) continue;
            out += p->primary ? "  >> " : "    ";
            out += std::string(p->sw) + " - " + std::string(p->desc) + "\r\n";
        }
    } else {
        out += "\xE8\xAF\x86\xE5\x88\xAB\xE7\xBB\x93\xE6\x9E\x9C: \xE6\x9C\xAA\xE8\xAF\x86\xE5\x88\xAB\r\n";
        out += "\xE6\xA3\x80\xE6\xB5\x8B\xE6\x96\xB9\xE5\xBC\x8F: " + result.detectedBy + "\r\n";
        out += "\xE9\x94\x99\xE8\xAF\xAF\xE4\xBF\xA1\xE6\x81\xAF: " + result.errorMessage + "\r\n";
    }

    FILE* f = _wfopen(outFile.c_str(), L"wb");
    if (!f) return 1;
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, f);
    fwrite(out.c_str(), 1, out.size(), f);
    fclose(f);
    return 0;
}

static bool IsValidFile(const std::wstring& path) {
    if (path.empty()) return false;
    size_t dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = path.substr(dot);
    for (auto& c : ext) c = towlower(c);
    return ext == L".exe" || ext == L".msi" || ext == L".appx" ||
           ext == L".msix" || ext == L".msixbundle";
}

static void SetStatus(const wchar_t* text) {
    if (g_hStatusBar)
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
}

static HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, DWORD style = 0) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT | style,
        x, y, w, h, parent, nullptr, g_hInst, nullptr);
}

static HWND CreateBtn(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
}

static HWND CreateChk(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool checked = true) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    if (checked) SendMessage(hw, BM_SETCHECK, BST_CHECKED, 0);
    return hw;
}

// ============================================================
// Layout
// ============================================================

static void LayoutControls(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;
    int pad = 12, grpW = W - 2*pad;
    int innerPad = 15;
    int y = pad;

    MoveWindow(g_hGrpDrop, pad, y, grpW, 100, TRUE);
    int gw = grpW - 2*innerPad;
    MoveWindow(g_hLblDrop, pad + innerPad, y + 20, gw - 90, 40, TRUE);
    MoveWindow(g_hBtnBrowse, pad + innerPad + gw - 80, y + 28, 80, 30, TRUE);
    MoveWindow(g_hLblFilePath, pad + innerPad, y + 68, gw, 23, TRUE);
    y += 106;

    MoveWindow(g_hGrpResult, pad, y, grpW, 100, TRUE);
    int col1x = pad + innerPad + 85;
    int col2x = pad + innerPad + 265;
    MoveWindow(g_hLblTypeCap, pad + innerPad, y + 25, 80, 20, TRUE);
    MoveWindow(g_hLblType, col1x, y + 25, 170, 20, TRUE);
    MoveWindow(g_hLblConfCap, col2x, y + 25, 60, 20, TRUE);
    MoveWindow(g_hLblConf, col2x + 60, y + 25, 120, 25, TRUE);
    int infoWidth = grpW - 2*innerPad - 85 - 10; // 修复：拓宽版本/检测方式标签显示DIE长文本
    MoveWindow(g_hLblVerCap, pad + innerPad, y + 50, 80, 20, TRUE);
    MoveWindow(g_hLblVer, col1x, y + 50, infoWidth, 20, TRUE);
    MoveWindow(g_hLblDetCap, pad + innerPad, y + 73, 80, 20, TRUE);
    MoveWindow(g_hLblDet, col1x, y + 73, infoWidth, 20, TRUE);
    y += 106;

    int paramsH = H - y - 216;
    if (paramsH < 120) paramsH = 120;
    MoveWindow(g_hGrpParams, pad, y, grpW, paramsH, TRUE);
    MoveWindow(g_hLstParams, pad + innerPad, y + 20, grpW - 2*innerPad, paramsH - 26, TRUE);
    y += paramsH + 6;

    MoveWindow(g_hGrpCmd, pad, y, grpW, 55, TRUE);
    MoveWindow(g_hTxtCmd, pad + innerPad, y + 20, grpW - 2*innerPad - 85, 23, TRUE);
    MoveWindow(g_hBtnCopy, pad + grpW - innerPad - 80, y + 18, 80, 27, TRUE);
    y += 61;

    MoveWindow(g_hGrpBatch, pad, y, grpW, 70, TRUE);
    int chkY = y + 22;
    int chkW = 100;
    MoveWindow(g_hChkLog, pad + innerPad, chkY, chkW, 22, TRUE);
    MoveWindow(g_hChkErr, pad + innerPad + chkW, chkY, chkW, 22, TRUE);
    MoveWindow(g_hChkUac, pad + innerPad + chkW*2, chkY, chkW + 10, 22, TRUE);
    MoveWindow(g_hChkComment, pad + innerPad + chkW*3 + 10, chkY, chkW, 22, TRUE);
    MoveWindow(g_hBtnGenBat, pad + grpW - innerPad - 140, y + 42, 140, 25, TRUE);
    y += 76;

    MoveWindow(g_hGrpRun, pad, y, grpW, 50, TRUE);
    int btnW = (grpW - 2*innerPad - 10) / 2;
    MoveWindow(g_hBtnRunExes, pad + innerPad, y + 20, btnW, 25, TRUE);
    MoveWindow(g_hBtnRunAll, pad + innerPad + btnW + 10, y + 20, btnW, 25, TRUE);

    // Status bar and progress bar at bottom
    SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    RECT rcStatus;
    GetWindowRect(g_hStatusBar, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;
    int progressW = W / 3; // 进度条占右边1/3
    int statusW = W - progressW;
    MoveWindow(g_hStatusBar, 0, H - statusH, statusW, statusH, TRUE);
    MoveWindow(g_hProgress, statusW, H - statusH + 2, progressW, statusH - 4, TRUE);
}

// ============================================================
// Scan file
// ============================================================

static void ScanFile(const std::wstring& filePath) {
    SetStatus(L"正在扫描...");
    SetWindowTextW(g_hLblFilePath, filePath.c_str());

    try {
        DetectorEngine scanner;
        g_currentResult = scanner.Scan(filePath);
        g_hasResult = g_currentResult.success;
    } catch (...) {
        g_hasResult = false;
        g_currentResult.success = false;
        g_currentResult.errorMessage = "Exception during scan";
    }

    if (g_hasResult) {
        std::wstring typeStr = AToW(g_currentResult.installerFullName)
            + L" (" + AToW(g_currentResult.installerType) + L")";
        SetWindowTextW(g_hLblType, typeStr.c_str());
        InvalidateRect(g_hLblType, NULL, TRUE);
        SetWindowTextW(g_hLblVer, L"--");
        InvalidateRect(g_hLblVer, NULL, TRUE);
        SetWindowTextW(g_hLblDet, AToW(g_currentResult.detectedBy).c_str());
        InvalidateRect(g_hLblDet, NULL, TRUE);
        SetWindowTextW(g_hLblConf, g_currentResult.GetConfidenceStars().c_str());
        InvalidateRect(g_hLblConf, NULL, TRUE);
        SetWindowTextW(g_hTxtCmd, AToW(g_currentResult.silentCommand).c_str());
        InvalidateRect(g_hTxtCmd, NULL, TRUE);

        ListView_DeleteAllItems(g_hLstParams);
        for (size_t i = 0; i < g_currentResult.params.size(); i++) {
            const SilentParam* p = g_currentResult.params[i];
            if (!p) continue;

            std::wstring displaySw = p->primary ? (L">> " + AToW(p->sw)) : AToW(p->sw);
            std::wstring desc = AToW(p->desc);

            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = (int)i;
            item.pszText = (LPWSTR)displaySw.c_str();
            ListView_InsertItem(g_hLstParams, &item);

            LVITEMW sub = {};
            sub.mask = LVIF_TEXT;
            sub.iItem = (int)i;
            sub.iSubItem = 1;
            sub.pszText = (LPWSTR)desc.c_str();
            ListView_SetItem(g_hLstParams, &sub);
        }

        EnableWindow(g_hBtnGenBat, TRUE);
        SetWindowTextW(g_hLblDrop, g_currentResult.fileName.c_str());
        SetStatus((L"扫描完成 - " + AToW(g_currentResult.installerFullName)).c_str());
    } else {
        // 修复：DIE检测到非安装器时，在"安装器"/"版本"行显示实际检测信息
        if (!g_currentResult.dieDetection.empty()) {
            SetWindowTextW(g_hLblType, L"非安装器程序"); // 非安装器程序
            InvalidateRect(g_hLblType, NULL, TRUE);
            std::wstring dieInfo = AToW(g_currentResult.dieDetection);
            SetWindowTextW(g_hLblVer, dieInfo.c_str());
            InvalidateRect(g_hLblVer, NULL, TRUE);
            SetWindowTextW(g_hLblDet, AToW(g_currentResult.detectedBy).c_str());
            InvalidateRect(g_hLblDet, NULL, TRUE);
            SetWindowTextW(g_hLblConf, L"--");
            InvalidateRect(g_hLblConf, NULL, TRUE);
        } else {
            SetWindowTextW(g_hLblType, L"未识别"); // 未识别
            InvalidateRect(g_hLblType, NULL, TRUE);
            SetWindowTextW(g_hLblVer, L"--");
            InvalidateRect(g_hLblVer, NULL, TRUE);
            SetWindowTextW(g_hLblDet, AToW(g_currentResult.detectedBy).c_str());
            InvalidateRect(g_hLblDet, NULL, TRUE);
            SetWindowTextW(g_hLblConf, L"--");
            InvalidateRect(g_hLblConf, NULL, TRUE);
        }
        SetWindowTextW(g_hTxtCmd, L"");
        InvalidateRect(g_hTxtCmd, NULL, TRUE);
        ListView_DeleteAllItems(g_hLstParams);
        EnableWindow(g_hBtnGenBat, FALSE);
        SetWindowTextW(g_hLblDrop, g_currentResult.fileName.c_str());
        SetStatus((L"扫描失败 - " + AToW(g_currentResult.errorMessage)).c_str()); // 扫描失败
    }
}

// ============================================================
// Create controls
// ============================================================

static void CreateControls(HWND hWnd) {
    g_hFontNormal = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_hFontBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_hFontMono = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");

    g_hBrushDrop = CreateSolidBrush(RGB(0xF0, 0xF8, 0xFF));

    // Group 1: File Selection
    g_hGrpDrop = CreateWindowExW(0, L"BUTTON", L"\x6587\x4EF6\x9009\x62E9", // 文件选择
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)IDC_GRP_DROP, g_hInst, nullptr);
    g_hLblDrop = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC",
        L"\x62D6\x653E EXE/MSI/APPX/MSIX \x6587\x4EF6\x5230\x6B64\x5904", // 拖放 EXE/MSI/APPX/MSIX 文件到此处
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        0,0,0,0, hWnd, (HMENU)IDC_LBL_DROP, g_hInst, nullptr);
    g_hBtnBrowse = CreateBtn(hWnd, L"\x6D4F\x89C8...", IDC_BTN_BROWSE, 0,0,0,0); // 浏览...
    g_hLblFilePath = CreateLabel(hWnd, L"", 0,0,0,0);

    // Group 2: Detection Results
    g_hGrpResult = CreateWindowExW(0, L"BUTTON", L"\x68C0\x6D4B\x7ED3\x679C", // 检测结果
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)IDC_GRP_RESULT, g_hInst, nullptr);
    g_hLblTypeCap = CreateLabel(hWnd, L"\x5B89\x88C5\x5668:", 0,0,0,0); // 安装器:
    g_hLblType = CreateLabel(hWnd, L"", 0,0,0,0);
    g_hLblVerCap = CreateLabel(hWnd, L"\x7248\x672C:", 0,0,0,0); // 版本:
    g_hLblVer = CreateLabel(hWnd, L"--", 0,0,0,0);
    g_hLblDetCap = CreateLabel(hWnd, L"\x68C0\x6D4B\x65B9\x5F0F:", 0,0,0,0); // 检测方式:
    g_hLblDet = CreateLabel(hWnd, L"", 0,0,0,0);
    g_hLblConfCap = CreateLabel(hWnd, L"\x7F6E\x4FE1\x5EA6:", 0,0,0,0); // 置信度:
    g_hLblConf = CreateLabel(hWnd, L"", 0,0,0,0);

    // Group 3: Parameters
    g_hGrpParams = CreateWindowExW(0, L"BUTTON", L"\x9759\x9ED8\x5B89\x88C5\x53C2\x6570", // 静默安装参数
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)IDC_GRP_PARAMS, g_hInst, nullptr);
    g_hLstParams = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0,0,0,0, hWnd, (HMENU)IDC_LST_PARAMS, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hLstParams,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = 200;
    col.pszText = (LPWSTR)L"\x53C2\x6570"; // 参数
    col.iSubItem = 0;
    ListView_InsertColumn(g_hLstParams, 0, &col);
    col.cx = 320;
    col.pszText = (LPWSTR)L"\x8BF4\x660E"; // 说明
    col.iSubItem = 1;
    ListView_InsertColumn(g_hLstParams, 1, &col);

    // Group 4: Command
    g_hGrpCmd = CreateWindowExW(0, L"BUTTON", L"\x9759\x9ED8\x5B89\x88C5\x547D\x4EE4", // 静默安装命令
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)IDC_GRP_CMD, g_hInst, nullptr);
    g_hTxtCmd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
        0,0,0,0, hWnd, (HMENU)IDC_TXT_CMD, g_hInst, nullptr);
    g_hBtnCopy = CreateBtn(hWnd, L"\x590D\x5236", IDC_BTN_COPY, 0,0,0,0); // 复制

    // Group 5: Batch options
    g_hGrpBatch = CreateWindowExW(0, L"BUTTON", L"\x6279\x5904\x7406\x751F\x6210\x9009\x9879", // 批处理生成选项
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)IDC_GRP_BATCH, g_hInst, nullptr);
    g_hChkLog = CreateChk(hWnd, L"\x5305\x542B\x65E5\x5FD7", IDC_CHK_LOG, 0,0,0,0); // 包含日志
    g_hChkErr = CreateChk(hWnd, L"\x9519\x8BEF\x5904\x7406", IDC_CHK_ERR, 0,0,0,0); // 错误处理
    g_hChkUac = CreateChk(hWnd, L"\x7BA1\x7406\x5458\x6743\x9650", IDC_CHK_UAC, 0,0,0,0); // 管理员权限
    g_hChkComment = CreateChk(hWnd, L"\x6CE8\x91CA", IDC_CHK_COMMENT, 0,0,0,0); // 注释
    g_hBtnGenBat = CreateBtn(hWnd, L"\x751F\x6210\x6279\x5904\x7406\x6587\x4EF6", IDC_BTN_GENBAT, 0,0,0,0); // 生成批处理文件
    EnableWindow(g_hBtnGenBat, FALSE);

    // Group 6: Batch Run
    g_hGrpRun = CreateWindowExW(0, L"BUTTON", L"\x6279\x91CF\x8FD0\x884C", // 批量运行
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,0,0,0, hWnd, (HMENU)1070, g_hInst, nullptr);
    g_hBtnRunExes = CreateBtn(hWnd, L"\x8FD0\x884C\x5168\x90E8\x6267\x884C\x7A0B\x5E8F", IDC_BTN_RUN_EXES, 0,0,0,0); // 运行全部执行程序
    g_hBtnRunAll = CreateBtn(hWnd, L"\x8FD0\x884C\x5168\x90E8\x7A0B\x5E8F\x548C\x811A\x672C", IDC_BTN_RUN_ALL, 0,0,0,0); // 运行全部程序和脚本

    // Progress bar (initially hidden)
    g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | PBS_SMOOTH,
        0,0,0,0, hWnd, (HMENU)IDC_PROGRESS, g_hInst, nullptr);
    SendMessage(g_hProgress, PBM_SETRANGE32, 0, 100);
    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0,0,0,0, hWnd, (HMENU)IDC_STATUSBAR, g_hInst, nullptr);
    int parts[] = {-1};
    SendMessage(g_hStatusBar, SB_SETPARTS, 1, (LPARAM)parts);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"\x5C31\x7EEA"); // 就绪
    // DIE status will be updated after Tools are extracted in wWinMain

    // Apply fonts
    HWND allLabels[] = {g_hGrpDrop, g_hGrpResult, g_hGrpParams, g_hGrpCmd, g_hGrpBatch, g_hGrpRun,
        g_hLblDrop, g_hBtnBrowse, g_hLblFilePath,
        g_hLblTypeCap, g_hLblType, g_hLblVerCap, g_hLblVer,
        g_hLblDetCap, g_hLblDet, g_hLblConfCap, g_hLblConf,
        g_hBtnCopy, g_hBtnGenBat, g_hBtnRunExes, g_hBtnRunAll,
        g_hChkLog, g_hChkErr, g_hChkUac, g_hChkComment};
    for (HWND h : allLabels) SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessage(g_hTxtCmd, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    SendMessage(g_hLstParams, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    DragAcceptFiles(hWnd, TRUE);
}

// ============================================================
// WndProc
// ============================================================

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        LayoutControls(hWnd);
        return 0;

    case WM_SIZE:
        LayoutControls(hWnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 620;
        mmi->ptMinTrackSize.y = 640;
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hStatic = (HWND)lParam;
        if (hStatic == g_hLblDrop) {
            SetBkColor(hdc, RGB(0xF0, 0xF8, 0xFF));
            return (LRESULT)g_hBrushDrop;
        }
        if (hStatic == g_hLblType) {
            SetTextColor(hdc, RGB(0, 0x50, 0xA0));
            SelectObject(hdc, g_hFontBold);
            SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        if (hStatic == g_hLblConf) {
            SetTextColor(hdc, RGB(0xC8, 0x78, 0x00));
            SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
            if (IsValidFile(path))
                ScanFile(path);
            else
                SetStatus(L"\x6587\x4EF6\x7C7B\x578B\x65E0\x6548\xFF0C\x8BF7\x62D6\x653E EXE/MSI/APPX/MSIX \x6587\x4EF6");
                // 文件类型无效，请拖放 EXE/MSI/APPX/MSIX 文件
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_BROWSE: {
            wchar_t path[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"\x5B89\x88C5\x5305\x6587\x4EF6 (*.exe;*.msi;*.appx;*.msix;*.msixbundle)\0*.exe;*.msi;*.appx;*.msix;*.msixbundle\0\x6240\x6709\x6587\x4EF6 (*.*)\0*.*\0";
            // 安装包文件 ... 所有文件 ...
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn))
                ScanFile(path);
            return 0;
        }

        case IDC_BTN_COPY: {
            if (g_hasResult) {
                std::wstring cmd = AToW(g_currentResult.silentCommand);
                if (OpenClipboard(hWnd)) {
                    EmptyClipboard();
                    size_t size = (cmd.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                    if (hMem) {
                        wchar_t* p = (wchar_t*)GlobalLock(hMem);
                        memcpy(p, cmd.c_str(), size);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();
                    SetStatus(L"\x547D\x4EE4\x5DF2\x590D\x5236\x5230\x526A\x8D34\x677F"); // 命令已复制到剪贴板
                }
            }
            return 0;
        }

        case IDC_BTN_GENBAT: {
            if (!g_hasResult) return 0;
            wchar_t path[MAX_PATH] = {};
            std::wstring defName = L"silent_install_";
            std::wstring fname = g_currentResult.fileName;
            size_t dot = fname.rfind(L'.');
            if (dot != std::wstring::npos) defName += fname.substr(0, dot);
            else defName += fname;
            defName += L".bat";
            wcscpy_s(path, defName.c_str());

            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"\x6279\x5904\x7406\x6587\x4EF6 (*.bat)\0*.bat\0"; // 批处理文件
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = L"bat";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
            if (GetSaveFileNameW(&ofn)) {
                BatchOptions opts;
                opts.includeLog = (SendMessage(g_hChkLog, BM_GETCHECK, 0, 0) == BST_CHECKED);
                opts.includeErrorHandling = (SendMessage(g_hChkErr, BM_GETCHECK, 0, 0) == BST_CHECKED);
                opts.includeUac = (SendMessage(g_hChkUac, BM_GETCHECK, 0, 0) == BST_CHECKED);
                opts.includeComments = (SendMessage(g_hChkComment, BM_GETCHECK, 0, 0) == BST_CHECKED);

                std::string content = GenerateBatch(g_currentResult, opts);
                if (SaveBatchFile(content, path)) {
                    std::wstring msg = L"\x6279\x5904\x7406\x6587\x4EF6\x5DF2\x751F\x6210:\n"
                        + std::wstring(path) + L"\n\n\x7F16\x7801: ANSI (GB2312)";
                    // 批处理文件已生成: ... 编码: ANSI (GB2312)
                    MessageBoxW(hWnd, msg.c_str(), L"\x5B8C\x6210", MB_OK | MB_ICONINFORMATION); // 完成
                    SetStatus((L"\x6279\x5904\x7406\x6587\x4EF6\x5DF2\x751F\x6210: " + std::wstring(path)).c_str());
                    // 批处理文件已生成: ...
                } else {
                    MessageBoxW(hWnd, L"\x4FDD\x5B58\x6587\x4EF6\x5931\x8D25\x3002", L"\x9519\x8BEF", MB_OK | MB_ICONERROR);
                    // 保存文件失败。 错误
                }
            }
            return 0;
        }

        case IDC_BTN_RUN_EXES: {
            std::wstring dir = BR_GetExeDirectory();
            BR_RunAll(hWnd, dir, RunFileType::ExeOnly);
            return 0;
        }

        case IDC_BTN_RUN_ALL: {
            std::wstring dir = BR_GetExeDirectory();
            BR_RunAll(hWnd, dir, RunFileType::AllRunnable);
            return 0;
        }
        } // end switch LOWORD(wParam)
        break;

    case WM_USER + 1: {
        // Tools extraction complete, update DIE status
        DieScanner die;
        std::wstring countStr;
        if (die.IsAvailable()) {
            countStr = L"\x5C31\x7EEA | \x652F\x6301: " + std::to_wstring(DB_GetCount()) + L" \x79CD\x5B89\x88C5\x5668 | DIE: OK";
        } else {
            countStr = L"\x5C31\x7EEA | DIE: \x672A\x627E\x5230\xFF0C\x652F\x6301\x7684\x5B89\x88C5\x5668\x6570\x91CF\x5C06\x5927\x5E45\x4E0B\x964D";
        }
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)countStr.c_str());
        return 0;
    }

    case WM_DESTROY:
        DeleteObject(g_hFontNormal);
        DeleteObject(g_hFontBold);
        DeleteObject(g_hFontMono);
        DeleteObject(g_hBrushDrop);
        if (g_hIcon) DestroyIcon(g_hIcon);

        // 修复：等待后台解压线程完成再清理资源，避免竞态条件
        if (g_extractFuture.valid()) {
            g_extractFuture.wait_for(std::chrono::seconds(10));
        }

        // Cleanup temp Tools directory (only if we extracted to temp)
        if (g_toolsExtractedToTemp) {
            wchar_t tempDir[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            std::wstring toolsDir = std::wstring(tempDir) + L"SilentParamQuery_Tools";
            toolsDir.push_back(L'\0'); // SHFileOperation要求双null结尾
            SHFILEOPSTRUCTW fop = {};
            fop.wFunc = FO_DELETE;
            fop.pFrom = toolsDir.c_str();
            fop.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            SHFileOperationW(&fop);
        }

        CoUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
// WinMain
// ============================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // Parse command line early (before window creation for headless mode)
    CliArgs cli = ParseCommandLine(lpCmdLine);

    // Initialize COM for Shell operations
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icex);

    // Initialize debug log
    InitLog();
    Log("=== Program START ===");

    // Load icon
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
    LogW(L"Exe dir: %s", exeDir.c_str());

    std::wstring iconPath = exeDir + L"\\app.ico";
    g_hIcon = (HICON)LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

    // Headless mode: /out:filename or -out:filename
    // Handle BEFORE creating any window — scan file, write results, exit
    if (!cli.outFile.empty()) {
        if (cli.inputFile.empty()) {
            MessageBoxW(nullptr,
                L"使用 /out:filename 时必须指定要扫描的文件。\n\n示例:\n  SilentParamQuery.exe /out:result.txt setup.exe",
                L"参数错误", MB_OK | MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        // Sync extract Tools
        if (!FindToolsInExeDir()) {
            ExtractToolsZip();
        }
        // SetToolsPath already called inside FindToolsInExeDir/ExtractToolsZip
        int result = HeadlessScan(cli.inputFile, cli.outFile);
        CoUninitialize();
        return result;
    }

    // Create window
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SilentParamQuery";
    if (g_hIcon) wc.hIcon = g_hIcon;
    RegisterClassExW(&wc);

    std::wstring title = L"\x5927\x5185\x9759\x63A2 v1.3.2"; // 大内静探 v1.3.2

    g_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"SilentParamQuery", title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 640,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Extract Tools in background thread to avoid UI freeze
    Log("Looking for Tools in exe dir...");
    bool foundTools = FindToolsInExeDir();
    Log("FindToolsInExeDir: %s", foundTools ? "FOUND" : "NOT FOUND");

    if (!foundTools) {
        Log("Starting background extraction...");
        SetStatus(L"\x6B63\x5728\x89E3\x538B\x5DE5\x5177\x7EC4\x4EF6...");

        g_extractFuture = std::async(std::launch::async, []() {
            bool extractOk = ExtractToolsZip();
            Log("ExtractToolsZip: %s", extractOk ? "OK" : "FAILED");
            // SetToolsPath + g_toolsExtractedToTemp already set inside ExtractToolsZip
            PostMessage(g_hWnd, WM_USER + 1, 0, 0);
        });
    } else {
        // SetToolsPath already called inside FindToolsInExeDir

        DieScanner die;
        std::wstring countStr;
        if (die.IsAvailable()) {
            countStr = L"\x5C31\x7EEA | \x652F\x6301: " + std::to_wstring(DB_GetCount()) + L" \x79CD\x5B89\x88C5\x5668 | DIE: OK";
        } else {
            countStr = L"\x5C31\x7EEA | DIE: \x672A\x627E\x5230\xFF0C\x652F\x6301\x7684\x5B89\x88C5\x5668\x6570\x91CF\x5C06\x5927\x5E45\x4E0B\x964D";
        }
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)countStr.c_str());
    }

    if (g_hIcon) {
        SendMessage(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
        SendMessage(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);
    }

    // GUI mode CLI actions: file scan + /copy
    if (!cli.inputFile.empty() && GetFileAttributesW(cli.inputFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
        ScanFile(cli.inputFile);

        if (cli.copyCmd && g_hasResult && !g_currentResult.silentCommand.empty()) {
            std::wstring cmd = AToW(g_currentResult.silentCommand);
            if (OpenClipboard(g_hWnd)) {
                EmptyClipboard();
                size_t size = (cmd.size() + 1) * sizeof(wchar_t);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hMem) {
                    wchar_t* p = (wchar_t*)GlobalLock(hMem);
                    memcpy(p, cmd.c_str(), size);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
        }
    }

    // /run or /all: trigger batch execution
    if (cli.runExes && !cli.runAll) {
        std::wstring dir = BR_GetExeDirectory();
        BR_RunAll(g_hWnd, dir, RunFileType::ExeOnly);
    } else if (cli.runAll) {
        std::wstring dir = BR_GetExeDirectory();
        BR_RunAll(g_hWnd, dir, RunFileType::AllRunnable);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
