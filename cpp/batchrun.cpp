#include "batchrun.h"
#include "scanner.h"
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include <cstdarg>
#include <cstdio>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>

#pragma comment(lib, "shlwapi.lib")

// 日志开关：定义DEBUG_LOG宏时输出debug_log.txt，否则日志函数为空操作
#ifdef DEBUG_LOG
static std::wstring g_batchLogPath;

static void InitBatchLog() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeDir = exeDir.substr(0, lastSlash);
    g_batchLogPath = exeDir + L"\\debug_log.txt";
}

static void Log(const char* fmt, ...) {
    if (g_batchLogPath.empty()) InitBatchLog();
    FILE* f = _wfopen(g_batchLogPath.c_str(), L"a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

static void LogW(const wchar_t* fmt, ...) {
    if (g_batchLogPath.empty()) InitBatchLog();
    FILE* f = _wfopen(g_batchLogPath.c_str(), L"a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfwprintf(f, fmt, args);
    va_end(args);
    fwprintf(f, L"\n");
    fclose(f);
}
#else
static inline void InitBatchLog() {}
static inline void Log(const char*, ...) {}
static inline void LogW(const wchar_t*, ...) {}
#endif

static std::vector<std::wstring> g_tempFiles;

static std::wstring ToLower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = towlower(c);
    return r;
}

// 判断文件名是否包含优先关键词（运行库等）
static bool HasPriorityKeyword(const std::wstring& nameLower) {
    static const wchar_t* keywords[] = {
        L"vc", L"vcredist", L"microsoft", L"runtime",
        L"dotnet", L"directx", L"微软", L"运行库"
    };
    for (const auto& kw : keywords) {
        if (nameLower.find(kw) != std::wstring::npos)
            return true;
    }
    return false;
}

// 判断文件名是否包含延后关键词（需要重启的程序）
static bool HasDelayKeyword(const std::wstring& nameLower) {
    static const wchar_t* keywords[] = {
        L"primo", L"极域"
    };
    for (const auto& kw : keywords) {
        if (nameLower.find(kw) != std::wstring::npos)
            return true;
    }
    return false;
}

// 判断是否为可执行文件（ExeOnly模式）
static bool IsExecutable(const std::wstring& ext) {
    static const wchar_t* exts[] = {
        L".exe", L".msi", L".appx", L".msix", L".msixbundle"
    };
    for (const auto& e : exts) {
        if (ext == e) return true;
    }
    return false;
}

// 判断是否为可运行文件（AllRunnable模式）
static bool IsRunnable(const std::wstring& ext) {
    if (IsExecutable(ext)) return true;
    static const wchar_t* exts[] = {
        L".vbs", L".bat", L".cmd", L".reg"
    };
    for (const auto& e : exts) {
        if (ext == e) return true;
    }
    return false;
}

std::wstring BR_GetExeDirectory() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        return path.substr(0, pos);
    return L".";
}

std::vector<RunItem> BR_ScanAndSort(const std::wstring& dir, RunFileType type) {
    std::vector<RunItem> items;

    // 获取自身文件名，用于排除
    wchar_t selfPath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    std::wstring selfName(selfPath);
    size_t selfPos = selfName.find_last_of(L"\\/");
    if (selfPos != std::wstring::npos)
        selfName = selfName.substr(selfPos + 1);
    selfName = ToLower(selfName);

    std::wstring search = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return items;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fname = fd.cFileName;

        // 排除自身
        if (ToLower(fname) == selfName) continue;

        std::wstring ext = L"";
        size_t dot = fname.rfind(L'.');
        if (dot != std::wstring::npos)
            ext = ToLower(fname.substr(dot));

        bool valid = (type == RunFileType::ExeOnly) ? IsExecutable(ext) : IsRunnable(ext);
        if (!valid) continue;

        std::wstring nameLower = ToLower(fname);
        int priority = 1;
        if (HasPriorityKeyword(nameLower)) priority = 0;
        else if (HasDelayKeyword(nameLower)) priority = 2;

        RunItem item;
        item.filePath = dir + L"\\" + fname;
        item.fileName = fname;
        item.ext = ext;
        item.priority = priority;
        items.push_back(item);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    // 排序：先按priority，再按文件名
    std::sort(items.begin(), items.end(), [](const RunItem& a, const RunItem& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return ToLower(a.fileName) < ToLower(b.fileName);
    });

    return items;
}

// 检测文件是否有UTF-8 BOM
static bool HasUtf8Bom(const std::wstring& filePath) {
    FILE* f = _wfopen(filePath.c_str(), L"rb");
    if (!f) return false;
    unsigned char bom[3] = {};
    size_t read = fread(bom, 1, 3, f);
    fclose(f);
    return (read >= 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
}

// 读取整个文件
static std::vector<unsigned char> ReadFileBytes(const std::wstring& filePath) {
    FILE* f = _wfopen(filePath.c_str(), L"rb");
    if (!f) return {};
    _fseeki64(f, 0, SEEK_END);
    __int64 fileSize = _ftelli64(f); // 修复：ftell对大文件溢出，改用_ftelli64
    if (fileSize <= 0) { fclose(f); return {}; }
    _fseeki64(f, 0, SEEK_SET);
    std::vector<unsigned char> data((size_t)fileSize);
    fread(data.data(), 1, (size_t)fileSize, f);
    fclose(f);
    return data;
}

// 不区分大小写查找
static size_t FindCaseInsensitive(const std::string& haystack, const std::string& needle, size_t start = 0) {
    if (needle.empty()) return start;
    std::string h = haystack;
    for (auto& c : h) c = tolower((unsigned char)c);
    std::string n = needle;
    for (auto& c : n) c = tolower((unsigned char)c);
    return h.find(n, start);
}

// 获取替换文本（运行时正确编码）
static std::string GetReplacement(bool isUtf8) {
    // "echo 暂停已消除"
    const wchar_t* wide = L"echo 暂停已消除";
    if (isUtf8) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, &s[0], len, nullptr, nullptr);
        return s;
    } else {
        int len = WideCharToMultiByte(936, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, '\0');
        WideCharToMultiByte(936, 0, wide, -1, &s[0], len, nullptr, nullptr);
        return s;
    }
}

std::wstring BR_PrepareBatCmd(const std::wstring& filePath) {
    // 读取原文件
    auto data = ReadFileBytes(filePath);
    if (data.empty()) return L"";

    bool isUtf8 = HasUtf8Bom(filePath);

    // 构建string用于搜索（跳过BOM）
    size_t bodyStart = isUtf8 ? 3 : 0;
    std::string body(data.begin() + bodyStart, data.end());

    // 检测是否有pause
    if (FindCaseInsensitive(body, "pause") == std::string::npos)
        return L"";

    // 替换pause
    std::string replacement = GetReplacement(isUtf8);
    size_t pos = 0;
    while ((pos = FindCaseInsensitive(body, "pause", pos)) != std::string::npos) {
        body.replace(pos, 5, replacement);
        pos += replacement.size();
    }

    // 生成同名文件，后面加"静默"
    // 例如：自用优化.bat -> 自用优化静默.bat
    std::wstring newPath = filePath;
    size_t dotPos = newPath.rfind(L'.');
    if (dotPos != std::wstring::npos)
        newPath = newPath.substr(0, dotPos) + L"\x9759\x9ED8" + newPath.substr(dotPos); // 静默
    else
        newPath += L"\x9759\x9ED8"; // 静默

    FILE* f = _wfopen(newPath.c_str(), L"wb");
    if (!f) return L"";

    if (isUtf8) {
        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        fwrite(bom, 1, 3, f);
    }
    fwrite(body.c_str(), 1, body.size(), f);
    fclose(f);

    g_tempFiles.push_back(newPath);
    return newPath;
}

void BR_CleanupTemp() {
    for (const auto& f : g_tempFiles)
        DeleteFileW(f.c_str());
    g_tempFiles.clear();
}

// ===== 压缩包处理功能 =====

// 检测7z是否可用
std::wstring BR_Find7z() {
    Log("=== BR_Find7z START ===");

    // 检查常见路径
    const wchar_t* paths[] = {
        L"%ProgramFiles%\\7-Zip\\7z.exe",
        L"%ProgramFiles(x86)%\\7-Zip\\7z.exe",
        L"C:\\Program Files\\7-Zip\\7z.exe",
        L"C:\\Program Files (x86)\\7-Zip\\7z.exe"
    };

    wchar_t expanded[MAX_PATH] = {};
    for (const auto& p : paths) {
        ExpandEnvironmentStringsW(p, expanded, MAX_PATH);
        LogW(L"Checking: %s -> %s", p, expanded);
        if (GetFileAttributesW(expanded) != INVALID_FILE_ATTRIBUTES) {
            LogW(L"Found 7z at: %s", expanded);
            return expanded;
        }
    }

    // 检查PATH环境变量
    wchar_t pathEnv[4096] = {};
    GetEnvironmentVariableW(L"PATH", pathEnv, 4096);
    std::wstring pathStr(pathEnv);
    LogW(L"PATH length: %d", (int)pathStr.size());

    size_t pos = 0;
    while (pos < pathStr.size()) {
        size_t next = pathStr.find(L';', pos);
        if (next == std::wstring::npos) next = pathStr.size();
        std::wstring dir = pathStr.substr(pos, next - pos);
        std::wstring exe = dir + L"\\7z.exe";
        if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES) {
            LogW(L"Found 7z in PATH: %s", exe.c_str());
            return exe;
        }
        pos = next + 1;
    }

    Log("7z NOT found anywhere!");
    return L"";
}

// 扫描目录下的7z文件
std::vector<std::wstring> BR_Scan7zFiles(const std::wstring& dir) {
    std::vector<std::wstring> files;
    std::wstring search = dir + L"\\*.7z";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return files;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(dir + L"\\" + fd.cFileName);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return files;
}

// 执行7z命令并获取输出
static std::wstring Run7zCommand(const std::wstring& sevenZPath, const std::wstring& args) {
    // 修复：句柄泄漏 — 所有路径确保关闭句柄；CreatePipe失败时检查返回值
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return L"";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"\"" + sevenZPath + L"\" " + args;
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    BOOL ok = CreateProcessW(NULL, buf.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return L"";
    }

    std::string output;
    char readBuf[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, readBuf, sizeof(readBuf), &bytesRead, NULL) && bytesRead > 0) {
        output.append(readBuf, bytesRead);
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // 转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) wideLen = MultiByteToWideChar(CP_ACP, 0, output.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return L"";
    std::wstring result(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, &result[0], wideLen);
    return result;
}

// 从压缩包名提取不带扩展名的名称
static std::wstring GetZipBaseName(const std::wstring& zipPath) {
    size_t lastSlash = zipPath.find_last_of(L"\\/");
    std::wstring fname = (lastSlash != std::wstring::npos) ? zipPath.substr(lastSlash + 1) : zipPath;
    size_t lastDot = fname.rfind(L'.');
    if (lastDot != std::wstring::npos)
        return fname.substr(0, lastDot);
    return fname;
}

// 判断exe是32位还是64位
static int GetExeArchitecture(const std::wstring& exePath) {
    FILE* f = _wfopen(exePath.c_str(), L"rb");
    if (!f) return -1;

    // 读取DOS头
    unsigned char dosHeader[64];
    if (fread(dosHeader, 1, 64, f) != 64) { fclose(f); return -1; }

    // 检查MZ签名
    if (dosHeader[0] != 'M' || dosHeader[1] != 'Z') { fclose(f); return -1; }

    // 获取PE头偏移
    DWORD peOffset = *(DWORD*)(dosHeader + 60);
    fseek(f, peOffset, SEEK_SET);

    // 读取PE签名和Machine字段
    unsigned char peHeader[6];
    if (fread(peHeader, 1, 6, f) != 6) { fclose(f); return -1; }

    // 检查PE签名
    if (peHeader[0] != 'P' || peHeader[1] != 'E' || peHeader[2] != 0 || peHeader[3] != 0) {
        fclose(f);
        return -1;
    }

    // Machine字段
    WORD machine = *(WORD*)(peHeader + 4);
    fclose(f);

    if (machine == 0x8664) return 64;  // AMD64
    if (machine == 0x014c) return 32;  // i386
    return -1;
}

// 解析7z l的输出，获取根目录下的文件和文件夹
struct SevenZEntry {
    bool isDir;
    std::wstring name;
};

static std::vector<SevenZEntry> Parse7zList(const std::wstring& output) {
    std::vector<SevenZEntry> entries;
    std::wistringstream stream(output);
    std::wstring line;
    bool inList = false;

    while (std::getline(stream, line)) {
        // 跳过头部，找到文件列表开始
        if (line.find(L"----------") != std::wstring::npos) {
            inList = !inList;
            continue;
        }

        if (!inList) continue;
        if (line.empty()) continue;

        // 解析行：日期 时间 属性 大小 压缩大小 文件名
        // 格式: 2026-04-15 12:38:43 D....            0            0  Weixin
        // 格式: 2026-04-15 12:38:43 ....A       519877    220111125  file.exe
        // 位置: 0-9=date, 10=space, 11-18=time, 19=space, 20-24=attr
        if (line.size() < 25) continue;

        // 检查属性列 (位置20开始，5个字符)
        size_t attrPos = 20;
        if (attrPos + 5 > line.size()) continue;

        bool isDir = (line[attrPos] == L'D');
        // 获取文件名（最后一列）
        // 跳过大小列，找到文件名
        size_t nameStart = line.rfind(L' ');
        if (nameStart == std::wstring::npos || nameStart <= attrPos + 10) continue;
        nameStart++;

        std::wstring name = line.substr(nameStart);
        if (name == L"." || name == L"..") continue;

        entries.push_back({isDir, name});
    }

    return entries;
}

// 在解压目录中查找主exe
static std::wstring FindMainExe(const std::wstring& dir, const std::wstring& zipBaseName) {
    std::vector<std::wstring> exes;

    // 扫描目录下的exe文件（不递归）
    std::wstring search = dir + L"\\*.exe";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                exes.push_back(fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    if (exes.empty()) return L"";

    // 只有一个exe，直接返回
    if (exes.size() == 1) return exes[0];

    // 多个exe，按优先级选择
    std::wstring zipLower = ToLower(zipBaseName);

    // 1. 与压缩包同名的exe
    for (const auto& e : exes) {
        if (ToLower(e) == zipLower + L".exe")
            return e;
    }

    // 2. 文件名包含压缩包名的exe
    for (const auto& e : exes) {
        if (ToLower(e).find(zipLower) != std::wstring::npos)
            return e;
    }

    // 3. 与压缩包名字最相似的exe（计算相似度）
    std::wstring bestMatch;
    int bestScore = -1;
    for (const auto& e : exes) {
        std::wstring eLower = ToLower(e);
        int score = 0;
        // 计算共同字符数
        for (size_t i = 0; i < zipLower.size() && i < eLower.size(); i++) {
            if (zipLower[i] == eLower[i]) score++;
            else break;
        }
        if (score > bestScore) {
            bestScore = score;
            bestMatch = e;
        }
    }
    if (!bestMatch.empty()) return bestMatch;

    // 4. 选最大的exe
    std::wstring largest;
    LARGE_INTEGER largestSize = {0};
    for (const auto& e : exes) {
        std::wstring fullPath = dir + L"\\" + e;
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER size;
            size.HighPart = fad.nFileSizeHigh;
            size.LowPart = fad.nFileSizeLow;
            if (size.QuadPart > largestSize.QuadPart) {
                largestSize = size;
                largest = e;
            }
        }
    }

    return largest;
}

// 创建桌面快捷方式
static bool CreateDesktopShortcut(const std::wstring& shortcutName,
                                   const std::wstring& targetPath,
                                   const std::wstring& workDir) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IShellLinkW* psl = NULL;
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    psl->SetPath(targetPath.c_str());
    psl->SetWorkingDirectory(workDir.c_str());
    psl->SetDescription(shortcutName.c_str());

    // 获取桌面路径
    wchar_t desktopPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);

    std::wstring lnkPath = std::wstring(desktopPath) + L"\\" + shortcutName + L".lnk";

    IPersistFile* ppf = NULL;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hr)) {
        hr = ppf->Save(lnkPath.c_str(), TRUE);
        ppf->Release();
    }

    psl->Release();
    CoUninitialize();
    return SUCCEEDED(hr);
}

// 处理单个7z文件
bool BR_Process7zFile(HWND hWnd, const std::wstring& zipPath, const std::wstring& sevenZPath) {
    Log("=== BR_Process7zFile START ===");

    // Convert wide strings to narrow for logging
    int len = WideCharToMultiByte(CP_UTF8, 0, zipPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string zipPathUtf8(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, zipPath.c_str(), -1, &zipPathUtf8[0], len, nullptr, nullptr);
    Log("zipPath: %s", zipPathUtf8.c_str());

    len = WideCharToMultiByte(CP_UTF8, 0, sevenZPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string sevenZPathUtf8(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sevenZPath.c_str(), -1, &sevenZPathUtf8[0], len, nullptr, nullptr);
    Log("sevenZPath: %s", sevenZPathUtf8.c_str());

    std::wstring zipBaseName = GetZipBaseName(zipPath);
    len = WideCharToMultiByte(CP_UTF8, 0, zipBaseName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string zipBaseNameUtf8(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, zipBaseName.c_str(), -1, &zipBaseNameUtf8[0], len, nullptr, nullptr);
    Log("zipBaseName: %s", zipBaseNameUtf8.c_str());

    // 1. 用 7z l 预检
    std::wstring listArgs = L"l \"" + zipPath + L"\"";
    std::wstring output = Run7zCommand(sevenZPath, listArgs);

    if (output.empty()) {
        Log("7z l command returned empty output!");
        return false;
    }
    Log("7z l command returned %d chars", (int)output.size());

    // Log raw 7z output for debugging
    Log("=== Raw 7z output (first 2000 chars) ===");
    int outLen = WideCharToMultiByte(CP_UTF8, 0, output.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string outputUtf8(outLen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, output.c_str(), -1, &outputUtf8[0], outLen, nullptr, nullptr);
    Log("%s", outputUtf8.substr(0, 2000).c_str());
    Log("=== End of raw output ===");

    // 2. 解析文件列表
    auto entries = Parse7zList(output);
    Log("Parsed %d entries", (int)entries.size());
    for (const auto& e : entries) {
        int elen = WideCharToMultiByte(CP_UTF8, 0, e.name.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string nameUtf8(elen - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, e.name.c_str(), -1, &nameUtf8[0], elen, nullptr, nullptr);
        Log("  Entry: %s %s", e.isDir ? "[DIR]" : "[FILE]", nameUtf8.c_str());
    }

    if (entries.empty()) {
        Log("No entries found in archive!");
        return false;
    }

    // 3. 检查根目录下是否有exe
    bool hasExeInRoot = false;
    for (const auto& e : entries) {
        if (!e.isDir) {
            size_t dot = e.name.rfind(L'.');
            if (dot != std::wstring::npos) {
                std::wstring ext = ToLower(e.name.substr(dot));
                if (ext == L".exe") {
                    hasExeInRoot = true;
                    break;
                }
            }
        }
    }
    Log("hasExeInRoot: %s", hasExeInRoot ? "YES" : "NO");

    // 4. 检查根目录下是否有文件夹包含exe
    bool hasFolderWithExe = false;
    for (const auto& e : entries) {
        if (e.isDir) {
            hasFolderWithExe = true;
            break;
        }
    }
    Log("hasFolderWithExe: %s", hasFolderWithExe ? "YES" : "NO");

    if (!hasExeInRoot && !hasFolderWithExe) {
        Log("No exe found in root or folders, SKIP");
        return false;
    }

    // 5. Extract to Program Files
    wchar_t pfDir[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pfDir);
    std::wstring extractDir = std::wstring(pfDir) + L"\\" + zipBaseName;

    // 检查目录是否已存在
    if (GetFileAttributesW(extractDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
        // 已存在，询问是否覆盖
        int ret = MessageBoxW(hWnd,
            (L"目标目录已存在：\n" + extractDir + L"\n\n是否删除后重新解压？").c_str(),
            L"确认", MB_YESNO | MB_ICONQUESTION);
        if (ret != IDYES) return false;

        // 修复：命令注入 — 用SHFileOperation替代cmd /c rmdir
        std::wstring delPath = extractDir + L'\0';
        SHFILEOPSTRUCTW fop = {};
        fop.wFunc = FO_DELETE;
        fop.pFrom = delPath.c_str();
        fop.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        SHFileOperationW(&fop);
    }

    // 创建目录
    CreateDirectoryW(extractDir.c_str(), NULL);

    // 解压
    Log("Extracting 7z archive...");
    std::wstring extractArgs = L"x -y -o\"" + extractDir + L"\" \"" + zipPath + L"\"";
    std::wstring extractOutput = Run7zCommand(sevenZPath, extractArgs);

    // 检查解压是否成功
    bool extractOk = (extractOutput.find(L"Everything is Ok") != std::wstring::npos ||
                      extractOutput.find(L"Ok") != std::wstring::npos);
    Log("Extract result: %s", extractOk ? "OK" : "FAILED");

    if (!extractOk) {
        // 解压失败，清理目录
        RemoveDirectoryW(extractDir.c_str());
        return false;
    }

    // 6. 查找主exe（递归查找子目录，最多3层）
    Log("Searching for main exe...");
    std::wstring mainExe;
    std::wstring finalExtractDir = extractDir;

    // Lambda for recursive search
    std::function<void(const std::wstring&, int)> searchExe;
    searchExe = [&](const std::wstring& dir, int depth) {
        if (!mainExe.empty() || depth > 3) return;

        // Check for exe in current directory
        std::wstring found = FindMainExe(dir, zipBaseName);
        if (!found.empty()) {
            int elen = WideCharToMultiByte(CP_UTF8, 0, found.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string foundUtf8(elen - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, found.c_str(), -1, &foundUtf8[0], elen, nullptr, nullptr);
            Log("Found exe at depth %d: %s", depth, foundUtf8.c_str());
            mainExe = found;
            finalExtractDir = dir;
            return;
        }

        // Search subdirectories
        std::wstring subSearch = dir + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(subSearch.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                searchExe(dir + L"\\" + fd.cFileName, depth + 1);
                if (!mainExe.empty()) break;
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    };

    searchExe(extractDir, 0);

    if (mainExe.empty()) {
        Log("No main exe found!");
        return false;
    }

    extractDir = finalExtractDir;
    Log("Using extract dir and main exe found");

    std::wstring fullExePath = extractDir + L"\\" + mainExe;

    // 7. 判断exe是32位还是64位
    int arch = GetExeArchitecture(fullExePath);
    if (arch == 32) {
        // 32位，移动到 Program Files (x86)
        wchar_t progFilesX86[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, progFilesX86);
        std::wstring newDir = std::wstring(progFilesX86) + L"\\" + zipBaseName;

        // 检查目标目录
        // 修复：命令注入 — 用SHFileOperation替代cmd /c rmdir
        if (GetFileAttributesW(newDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring delPath = newDir + L'\0';
            SHFILEOPSTRUCTW fop = {};
            fop.wFunc = FO_DELETE;
            fop.pFrom = delPath.c_str();
            fop.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            SHFileOperationW(&fop);
        }

        // 移动目录
        if (MoveFileW(extractDir.c_str(), newDir.c_str())) {
            extractDir = newDir;
            fullExePath = extractDir + L"\\" + mainExe;
        }
    }

    // 8. 创建桌面快捷方式
    LogW(L"Creating shortcut: %s -> %s", zipBaseName.c_str(), fullExePath.c_str());
    bool shortcutOk = CreateDesktopShortcut(zipBaseName, fullExePath, extractDir);
    Log("Shortcut created: %s", shortcutOk ? "OK" : "FAILED");

    return shortcutOk;
}

// 处理所有7z文件
int BR_ProcessAll7z(HWND hWnd, const std::wstring& dir) {
    Log("=== BR_ProcessAll7z START ===");
    LogW(L"Directory: %s", dir.c_str());

    // 1. 检测7z
    std::wstring sevenZPath = BR_Find7z();
    if (sevenZPath.empty()) {
        Log("7z not found!");
        HWND hStatus = FindWindowExW(hWnd, NULL, STATUSCLASSNAMEW, NULL);
        if (hStatus) {
            SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)L"未找到7z软件，不能处理压缩包");
        }
        return 0;
    }
    LogW(L"7z found at: %s", sevenZPath.c_str());

    // 2. 扫描7z文件
    auto zipFiles = BR_Scan7zFiles(dir);
    if (zipFiles.empty()) return 0;

    HWND hStatus = FindWindowExW(hWnd, NULL, STATUSCLASSNAMEW, NULL);

    int successCount = 0;
    for (size_t i = 0; i < zipFiles.size(); i++) {
        // 更新状态
        if (hStatus) {
            std::wstring status = L"正在处理压缩包: " + GetZipBaseName(zipFiles[i]);
            SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)status.c_str());
            UpdateWindow(hWnd);
        }

        bool ok = BR_Process7zFile(hWnd, zipFiles[i], sevenZPath);
        if (ok) successCount++;

        // 处理消息，避免界面卡死
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return successCount;
}

// 将UTF-8字符串转换为宽字符串
static std::wstring UTF8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

// 启动单个程序，返回进程句柄（调用者负责关闭）
static HANDLE LaunchItem(const RunItem& item, const std::wstring& dir) {
    std::wstring cmdLine;
    std::wstring workDir = dir;

    // bat/cmd：用 cmd /c 调用
    if (item.ext == L".bat" || item.ext == L".cmd") {
        std::wstring runPath = item.filePath;
        std::wstring silentPath = BR_PrepareBatCmd(item.filePath);
        if (!silentPath.empty())
            runPath = silentPath;
        cmdLine = L"cmd /c \"" + runPath + L"\"";
    }
    // reg：用 regedit /s
    else if (item.ext == L".reg") {
        cmdLine = L"regedit /s \"" + item.filePath + L"\"";
    }
    // vbs：用 wscript.exe
    else if (item.ext == L".vbs") {
        cmdLine = L"wscript.exe \"" + item.filePath + L"\"";
    }
    // exe/msi/appx/msix等：扫描静默参数并使用
    else {
        DetectorEngine scanner;
        ScanResult result = scanner.Scan(item.filePath);

        if (result.success && !result.silentCommand.empty()) {
            // 使用静默参数
            cmdLine = UTF8ToWide(result.silentCommand);
            // 替换 {file} 为实际文件路径
            size_t pos = cmdLine.find(L"{file}");
            if (pos != std::wstring::npos)
                cmdLine.replace(pos, 6, item.filePath);
        } else {
            // 未识别，普通运行
            cmdLine = L"\"" + item.filePath + L"\"";
        }
    }

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    BOOL ok = CreateProcessW(
        NULL, buf.data(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, workDir.c_str(), &si, &pi);

    if (ok) {
        CloseHandle(pi.hThread);
        return pi.hProcess;
    }
    return NULL;
}

int BR_RunAll(HWND hWnd, const std::wstring& dir, RunFileType type) {
    auto items = BR_ScanAndSort(dir, type);
    auto zipFiles = BR_Scan7zFiles(dir);
    int totalCount = (int)items.size() + (int)zipFiles.size();

    if (totalCount == 0) {
        MessageBoxW(hWnd, L"未找到可执行文件或压缩包。", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 确认对话框
    std::wstring msg = L"即将运行以下 " + std::to_wstring(totalCount) + L" 个文件（最多3路并行）：\n\n";
    for (size_t i = 0; i < items.size() && i < 20; i++) {
        msg += std::to_wstring(i + 1) + L". " + items[i].fileName + L"\n";
    }
    for (size_t i = 0; i < zipFiles.size() && (items.size() + i) < 20; i++) {
        size_t lastSlash = zipFiles[i].find_last_of(L"\\/");
        std::wstring fname = (lastSlash != std::wstring::npos) ? zipFiles[i].substr(lastSlash + 1) : zipFiles[i];
        msg += std::to_wstring(items.size() + i + 1) + L". " + fname + L"\n";
    }
    if (totalCount > 20) msg += L"... 等\n";
    msg += L"\n是否继续？";

    int ret = MessageBoxW(hWnd, msg.c_str(), L"确认执行",
        MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (ret != IDOK) return 0;

    HWND hStatus = FindWindowExW(hWnd, NULL, STATUSCLASSNAMEW, NULL);
    HWND hProgress = GetDlgItem(hWnd, 1062); // IDC_PROGRESS
    const int MAX_PARALLEL = 3;
    int successCount = 0;
    size_t nextIdx = 0;

    // 初始化进度条（包含exe/脚本 + 压缩包总数）
    if (hProgress) {
        SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)totalCount);
        SendMessage(hProgress, PBM_SETPOS, 0, 0);
        ShowWindow(hProgress, SW_SHOW);
    }

    // 运行中的进程：句柄 + 对应的文件索引
    struct RunningProc {
        HANDLE hProcess;
        size_t itemIdx;
    };
    std::vector<RunningProc> running;

    auto updateStatus = [&](const std::wstring& currentFile = L"") {
        if (hStatus) {
            std::wstring status;
            if (!currentFile.empty()) {
                status = L"\x6B63\x5728\x8FD0\x884C: " + currentFile; // 正在运行:
            } else {
                status = L"\x8FDB\x5EA6: " + std::to_wstring(successCount) + L"/" + // 进度:
                    std::to_wstring(totalCount);
            }
            SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)status.c_str());
        }
        if (hProgress) {
            SendMessage(hProgress, PBM_SETPOS, (WPARAM)successCount, 0);
        }
        // 强制刷新界面
        UpdateWindow(hWnd);
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    };

    while (successCount < (int)items.size()) {
        // 启动新进程，直到达到并行上限或没有更多任务
        while ((int)running.size() < MAX_PARALLEL && nextIdx < items.size()) {
            const auto& item = items[nextIdx];
            updateStatus(item.fileName); // 显示正在启动的文件
            HANDLE hProc = LaunchItem(item, dir);
            if (hProc) {
                running.push_back({hProc, nextIdx});
            } else {
                // 启动失败，跳过
                successCount++;
            }
            nextIdx++;
            Sleep(150); // 短暂间隔，避免瞬间压力
        }

        if (running.empty()) break;

        // 等待任意一个进程结束
        std::vector<HANDLE> handles;
        for (auto& r : running) handles.push_back(r.hProcess);

        DWORD waitResult = WaitForMultipleObjects(
            (DWORD)handles.size(), handles.data(), FALSE, 1000);

        if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handles.size()) {
            // 某个进程结束了
            size_t idx = waitResult - WAIT_OBJECT_0;
            CloseHandle(running[idx].hProcess);
            running.erase(running.begin() + idx);
            successCount++;
            // 显示刚完成的文件
            updateStatus();
        }
        // WAIT_TIMEOUT：继续循环检查
    }

    // 等待剩余进程
    for (auto& r : running) {
        WaitForSingleObject(r.hProcess, INFINITE);
        CloseHandle(r.hProcess);
        successCount++;
    }

    BR_CleanupTemp();
    updateStatus();

    // 处理压缩包（在弹出完成信息之前）
    int zipSuccess = BR_ProcessAll7z(hWnd, dir);
    successCount += zipSuccess;

    // 更新进度条到总数
    if (hProgress) {
        SendMessage(hProgress, PBM_SETPOS, (WPARAM)totalCount, 0);
    }

    // 完成状态
    if (hStatus) {
        std::wstring status = L"执行完成: " + std::to_wstring(successCount) +
            L"/" + std::to_wstring(totalCount) + L" 成功";
        SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)status.c_str());
    }

    std::wstring doneMsg = L"执行完成。\n成功: " + std::to_wstring(successCount) +
        L" / " + std::to_wstring(totalCount);
    MessageBoxW(hWnd, doneMsg.c_str(), L"完成", MB_OK | MB_ICONINFORMATION);

    return successCount;
}
