#include "scanner.h"
#include <algorithm>
#include <cstring>
#include <cctype>

// Global Tools path
static std::wstring g_toolsPath;

void SetToolsPath(const std::wstring& path) {
    g_toolsPath = path;
}

std::wstring GetToolsPath() {
    return g_toolsPath;
}

// ============================================================
// Helpers (standalone, shared by PeScanner and DieScanner)
// ============================================================

static std::string WToA(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr);
    return s;
}

static std::string GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return WToA((pos != std::wstring::npos) ? path.substr(pos + 1) : path);
}

static std::string ReplaceTemplate(const std::string& tmpl, const std::string& file) {
    std::string result = tmpl;
    size_t pos = result.find("{file}");
    if (pos != std::string::npos)
        result.replace(pos, 6, file);
    return result;
}

bool PeScanner::MatchBytes(const std::vector<BYTE>& data, size_t offset, const BYTE* pattern, size_t patLen) {
    if (offset + patLen > data.size() || patLen == 0) return false;
    for (size_t i = 0; i < patLen; i++)
        if (data[offset + i] != pattern[i]) return false;
    return true;
}

// Case-insensitive search in byte buffer (no null terminator issues)
static const BYTE* memistr(const BYTE* haystack, size_t hayLen, const char* needle) {
    if (!needle || !*needle) return nullptr;
    size_t nlen = strlen(needle);
    if (nlen > hayLen) return nullptr;
    for (size_t i = 0; i <= hayLen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower(haystack[i + j]) != tolower((BYTE)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return haystack + i;
    }
    return nullptr;
}

// Word-boundary-aware case-insensitive search (for short signatures < 6 chars)
// Requires non-alphanumeric chars (or start/end of buffer) around the match
static bool memistr_word(const BYTE* haystack, size_t hayLen, const char* needle) {
    if (!needle || !*needle) return false;
    size_t nlen = strlen(needle);
    if (nlen > hayLen) return false;
    for (size_t i = 0; i <= hayLen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower(haystack[i + j]) != tolower((BYTE)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            // Check word boundary before match
            if (i > 0 && isalnum(haystack[i - 1])) continue;
            // Check word boundary after match
            if (i + nlen < hayLen && isalnum(haystack[i + nlen])) continue;
            return true;
        }
    }
    return false;
}

// ============================================================
// Read file (max 4MB)
// ============================================================

std::vector<BYTE> PeScanner::ReadFileData(const std::wstring& filePath) {
    FILE* f = _wfopen(filePath.c_str(), L"rb");
    if (!f) return {};
    _fseeki64(f, 0, SEEK_END);
    __int64 fileSize = _ftelli64(f); // 修复：ftell对大文件(>2GB)溢出，改用_ftelli64
    if (fileSize <= 0) { fclose(f); return {}; }
    size_t readSize = (size_t)std::min((__int64)MAX_SCAN_SIZE, fileSize);
    fseek(f, 0, SEEK_SET);
    std::vector<BYTE> data(readSize);
    size_t bytesRead = fread(data.data(), 1, readSize, f);
    fclose(f);
    data.resize(bytesRead);
    return data;
}

// ============================================================
// Format checks
// ============================================================

bool PeScanner::IsMSI(const std::vector<BYTE>& data) {
    if (data.size() < 8) return false;
    return data[0]==0xD0 && data[1]==0xCF && data[2]==0x11 && data[3]==0xE0
        && data[4]==0xA1 && data[5]==0xB1 && data[6]==0x1A && data[7]==0xE1;
}

bool PeScanner::IsZIP(const std::vector<BYTE>& data) {
    if (data.size() < 4) return false;
    return data[0]==0x50 && data[1]==0x4B && data[2]==0x03 && data[3]==0x04;
}

ScanResult PeScanner::BuildMSIResult(ScanResult result) {
    const InstallerInfo* info = DB_FindByType("MSI");
    if (info) {
        result.installerType = info->type;
        result.installerFullName = info->fullName;
        result.confidence = 0.99;
        result.silentCommand = ReplaceTemplate(info->cmdTemplate, GetFileName(result.filePath));
        for (int i = 0; i < info->paramCount; i++)
            result.params.push_back(&info->params[i]);
        result.success = true;
    }
    return result;
}

ScanResult PeScanner::ScanZipFormat(const std::vector<BYTE>& data, ScanResult result) {
    if (!IsZIP(data)) return result;

    const char* zipTypes[] = {"MSIXBundle", "MSIX", "APPX"};
    for (const char* typeName : zipTypes) {
        const InstallerInfo* info = DB_FindByType(typeName);
        if (!info) continue;

        int matchCount = 0;
        for (int j = 0; j < info->strSigCount; j++) {
            size_t sigLen = strlen(info->strSigs[j]);
            bool found = (sigLen < 6)
                ? memistr_word(data.data(), data.size(), info->strSigs[j])
                : (memistr(data.data(), data.size(), info->strSigs[j]) != nullptr);
            if (found)
                matchCount++;
        }
        if (matchCount > 0) {
            result.installerType = info->type;
            result.installerFullName = info->fullName;
            result.confidence = std::min(0.95, 0.6 + matchCount * 0.15);
            result.silentCommand = ReplaceTemplate(info->cmdTemplate, GetFileName(result.filePath));
            for (int i = 0; i < info->paramCount; i++)
                result.params.push_back(&info->params[i]);
            result.success = true;
            return result;
        }
    }
    return result;
}

// ============================================================
// Detection strategies
// ============================================================

const InstallerInfo* PeScanner::ScanMagicBytes(const std::vector<BYTE>& data) {
    for (int i = 0; i < g_dbCount; i++) {
        const InstallerInfo& info = g_database[i];
        for (int j = 0; j < info.magicCount; j++) {
            const MagicBytes& mb = info.magics[j];
            if (mb.len == 0 || mb.data == nullptr) continue;
            size_t mlen = (size_t)mb.len;
            if (mlen > data.size()) continue;

            // Search overlay (last 1MB)
            size_t searchStart = (data.size() > 1024*1024) ? data.size() - 1024*1024 : 0;
            for (size_t k = searchStart; k + mlen <= data.size(); k++) {
                if (MatchBytes(data, k, mb.data, mlen))
                    return &info;
            }
            // Search header (first 4096)
            size_t headerEnd = std::min((size_t)4096, data.size());
            for (size_t k = 0; k + mlen <= headerEnd; k++) {
                if (MatchBytes(data, k, mb.data, mlen))
                    return &info;
            }
        }
    }
    return nullptr;
}

void PeScanner::ScanStrings(const std::vector<BYTE>& data, std::vector<std::pair<const InstallerInfo*, double>>& results) {
    // Search raw bytes directly — no std::string conversion, no null terminator issues
    // Short signatures (< 6 chars) use word-boundary matching to avoid false positives
    for (int i = 0; i < g_dbCount; i++) {
        const InstallerInfo& info = g_database[i];
        int matchCount = 0;
        int longMatchCount = 0; // signatures >= 10 chars (high confidence)
        for (int j = 0; j < info.strSigCount; j++) {
            size_t sigLen = strlen(info.strSigs[j]);
            bool found = (sigLen < 6)
                ? memistr_word(data.data(), data.size(), info.strSigs[j])
                : (memistr(data.data(), data.size(), info.strSigs[j]) != nullptr);
            if (found) {
                matchCount++;
                if (sigLen >= 10) longMatchCount++;
            }
        }
        // Require at least 2 matches, or 1 match with a long signature (>= 10 chars)
        if (matchCount >= 2 || (matchCount >= 1 && longMatchCount >= 1)) {
            double conf = std::min(0.9, 0.5 + matchCount * 0.15);
            results.push_back({&info, conf});
        }
    }
}

const InstallerInfo* PeScanner::ScanSections(const std::vector<BYTE>& data) {
    if (data.size() < 0x40) return nullptr;

    // Safe read of PE offset
    DWORD peOffset = 0;
    memcpy(&peOffset, &data[0x3C], 4);
    // 修复：peOffset负数/超大值校验 — 恶意PE构造导致越界
    if (peOffset > (DWORD)(data.size() - 24)) return nullptr;
    if (data[peOffset] != 0x50 || data[peOffset+1] != 0x45) return nullptr; // "PE"

    WORD numSections = 0;
    memcpy(&numSections, &data[peOffset + 6], 2);
    WORD optHeaderSize = 0;
    memcpy(&optHeaderSize, &data[peOffset + 20], 2);

    size_t sectionStart = peOffset + 24 + optHeaderSize;
    if (sectionStart > data.size()) return nullptr;

    for (int i = 0; i < numSections && i < 96; i++) {
        size_t secOff = sectionStart + (size_t)i * 40;
        if (secOff + 8 > data.size()) break;

        char name[9] = {0};
        memcpy(name, &data[secOff], 8);

        for (int j = 0; j < g_dbCount; j++) {
            const InstallerInfo& info = g_database[j];
            for (int k = 0; k < info.secNameCount; k++) {
                if (info.secNames[k] && _stricmp(name, info.secNames[k]) == 0)
                    return &info;
            }
        }
    }
    return nullptr;
}

bool PeScanner::HasEmbeddedMSI(const std::vector<BYTE>& data) {
    if (data.size() < 0x1008) return false;
    BYTE msiMagic[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    for (size_t i = 0x1000; i + 8 <= data.size(); i++) {
        if (MatchBytes(data, i, msiMagic, 8))
            return true;
    }
    return false;
}

// ============================================================
// Main scan
// ============================================================

ScanResult PeScanner::Scan(const std::wstring& filePath) {
    ScanResult result;
    result.filePath = filePath;
    result.detectedBy = "PeScanner";

    // Extract filename
    size_t sepPos = filePath.find_last_of(L"\\/");
    result.fileName = (sepPos != std::wstring::npos) ? filePath.substr(sepPos + 1) : filePath;

    std::string fname = GetFileName(filePath);

    // Read file
    std::vector<BYTE> data;
    try {
        data = ReadFileData(filePath);
    } catch (...) {
        result.errorMessage = "Exception reading file";
        return result;
    }

    if (data.empty()) {
        result.errorMessage = "Cannot read file or file is empty";
        return result;
    }

    if (data.size() < 64) {
        if (IsMSI(data)) return BuildMSIResult(result);
        ScanResult zipResult = ScanZipFormat(data, result);
        if (zipResult.success) return zipResult;
        result.errorMessage = "File too small, not a valid PE file";
        return result;
    }

    // Check MZ header
    if (data[0] != 0x4D || data[1] != 0x5A) {
        if (IsMSI(data)) return BuildMSIResult(result);
        ScanResult zipResult = ScanZipFormat(data, result);
        if (zipResult.success) return zipResult;
        result.errorMessage = "Not a valid PE file (MZ signature mismatch)";
        return result;
    }

    // Collect detections
    struct Detection { const InstallerInfo* info; double confidence; };
    std::vector<Detection> detected;

    try {
        // Strategy A: Magic bytes
        const InstallerInfo* magicResult = ScanMagicBytes(data);
        if (magicResult)
            detected.push_back({magicResult, 0.95});

        // Strategy B: String signatures
        std::vector<std::pair<const InstallerInfo*, double>> stringResults;
        ScanStrings(data, stringResults);
        for (auto& sr : stringResults)
            detected.push_back({sr.first, sr.second});

        // Strategy C: PE section names
        const InstallerInfo* sectionResult = ScanSections(data);
        if (sectionResult)
            detected.push_back({sectionResult, 0.7});

        // Remove magic-byte-only detections: if magic matched but no string/section
        // corroborates the same type, discard to avoid false positives
        // from common byte patterns like 0xDEADBEEF
        if (magicResult) {
            bool hasSupport = false;
            for (auto& d : detected) {
                if (d.info == magicResult && d.confidence < 0.95) {
                    hasSupport = true;
                    break;
                }
            }
            if (!hasSupport) {
                detected.erase(
                    std::remove_if(detected.begin(), detected.end(),
                        [magicResult](const Detection& d) {
                            return d.info == magicResult && d.confidence >= 0.95;
                        }),
                    detected.end());
            }
        }

        // Strategy D: Embedded MSI
        if (HasEmbeddedMSI(data)) {
            const InstallerInfo* msiWrap = DB_FindByType("MSIWrappedEXE");
            if (msiWrap)
                detected.push_back({msiWrap, 0.6});
        }
    } catch (...) {
        result.errorMessage = "Exception during scanning";
        return result;
    }

    if (detected.empty()) {
        result.errorMessage = "Installer type not recognized";
        return result;
    }

    // Pick highest confidence
    auto best = std::max_element(detected.begin(), detected.end(),
        [](const Detection& a, const Detection& b) { return a.confidence < b.confidence; });

    result.installerType = best->info->type;
    result.installerFullName = best->info->fullName;
    result.confidence = best->confidence;
    result.silentCommand = ReplaceTemplate(best->info->cmdTemplate, fname);
    for (int i = 0; i < best->info->paramCount; i++)
        result.params.push_back(&best->info->params[i]);
    result.success = true;

    return result;
}

// ============================================================
// DieScanner - DIE (Detect It Easy) integration
// ============================================================

DieScanner::DieScanner() {
    // First check if g_toolsPath is set (from embedded resource)
    if (!g_toolsPath.empty()) {
        m_diePath = g_toolsPath + L"\\diec.exe";
        if (GetFileAttributesW(m_diePath.c_str()) != INVALID_FILE_ATTRIBUTES)
            return;
    }

    // Look for diec.exe in Tools subdirectory relative to the EXE
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        exeDir = exeDir.substr(0, lastSlash);

    m_diePath = exeDir + L"\\Tools\\diec.exe";
    if (GetFileAttributesW(m_diePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Try parent directory's Tools folder
        m_diePath = exeDir + L"\\..\\Tools\\diec.exe";
        if (GetFileAttributesW(m_diePath.c_str()) == INVALID_FILE_ATTRIBUTES)
            m_diePath.clear();
    }
}

bool DieScanner::IsAvailable() const {
    return !m_diePath.empty();
}

std::string DieScanner::MapDieType(const std::string& dieType) {
    static const struct { const char* die; const char* internal; } mapping[] = {
        {"NSIS", "NSIS"},
        {"Inno Setup", "InnoSetup"},
        {"InstallShield", "InstallShield"},
        {"MSI", "MSI"},
        {"WiX", "WiXBurn"},
        {"Advanced Installer", "AdvancedInstaller"},
        {"Setup Factory", "SetupFactory"},
        {"Smart Install Maker", "SmartInstallMaker"},
        {"Ghost Installer", "GhostInstaller"},
        {"CreateInstall", "CreateInstall"},
        {"Actual Installer", "ActualInstaller"},
        {"SetupBuilder", "SetupBuilder"},
        {"InstallAware", "InstallAware"},
        {"Wise", "WiseInstaller"},
        {"RAR", "RARSFX"},
        {"7-Zip", "7ZipSFX"},
        {"7z SFX", "7ZipSFX"},
        {"Clickteam", "ClickteamIC"},
        {"Paquet Builder", "PaquetBuilder"},
        {"AutoPlay", "AutoPlayMS"},
        {"IExpress", "IExpress"},
        {"WExtract", "IExpress"},
        {"Squirrel", "Squirrel"},
        {"BitRock", "BitRockInstallBuilder"},
        {"InstallScript", "InstallScript"},
        {"WinZip", "WinZipSFX"},
        {"WiX MSI", "WiXMSI"},
        {"APPX", "APPX"},
        {"MSIX", "MSIX"},
        {"MSIXBundle", "MSIXBundle"},
    };
    for (const auto& m : mapping) {
        if (_stricmp(dieType.c_str(), m.die) == 0)
            return m.internal;
    }
    return dieType;
}

std::string DieScanner::ExtractVersion(const std::string& json, size_t matchIdx) {
    size_t start = (matchIdx > 10) ? matchIdx - 10 : 0;
    size_t end = std::min(json.size(), matchIdx + 100);
    std::string segment = json.substr(start, end - start);

    for (size_t i = 0; i + 2 < segment.size(); i++) {
        if (isdigit((unsigned char)segment[i]) && segment[i + 1] == '.') {
            size_t j = i;
            while (j < segment.size() && (isdigit((unsigned char)segment[j]) || segment[j] == '.'))
                j++;
            std::string ver = segment.substr(i, j - i);
            if (ver.size() >= 3 && ver.find('.') != std::string::npos)
                return ver;
        }
    }
    return "";
}

ScanResult DieScanner::Scan(const std::wstring& filePath) {
    ScanResult result;
    result.filePath = filePath;
    size_t sepPos = filePath.find_last_of(L"\\/");
    result.fileName = (sepPos != std::wstring::npos) ? filePath.substr(sepPos + 1) : filePath;
    result.detectedBy = "DIE";

    if (!IsAvailable()) {
        result.errorMessage = "diec.exe \xe4\xb8\x8d\xe5\x8f\xaf\xe7\x94\xa8"; // 不可用
        return result;
    }

    // Build command line
    // 修复：命令注入 — 转义文件路径中的双引号
    std::wstring safePath = filePath;
    size_t qPos = 0;
    while ((qPos = safePath.find(L'"', qPos)) != std::wstring::npos) {
        safePath.insert(qPos, L"\\");
        qPos += 2;
    }
    std::wstring cmd = L"\"" + m_diePath + L"\" --json \"" + safePath + L"\"";

    // Create pipes for stdout
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result.errorMessage = "\xe5\x88\x9b\xe5\xbb\xba\xe7\xae\xa1\xe9\x81\x93\xe5\xa4\xb1\xe8\xb4\xa5"; // 创建管道失败
        return result;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        result.errorMessage = "\xe5\x90\xaf\xe5\x8a\xa8 diec.exe \xe5\xa4\xb1\xe8\xb4\xa5"; // 启动 diec.exe 失败
        return result;
    }

    // Read stdout
    std::string output;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0)
        output.append(buf, bytesRead);
    CloseHandle(hRead);

    // Wait for process with timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 15000);
    // 修复：超时未退出 — 强制终止进程
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        result.errorMessage = "DIE \xe6\x89\xab\xe6\x8f\x8f\xe8\xb6\x85\xe6\x97\xb6 (15\xe7\xa7\x92)\xef\xbc\x8c\xe5\xb7\xb2\xe7\xbb\x88\xe6\xad\xa2"; // DIE扫描超时(15秒)已终止
        return result;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (output.empty()) {
        result.errorMessage = "diec.exe \xe8\xbe\x93\xe5\x87\xba\xe4\xb8\xba\xe7\xa9\xba"; // diec.exe 输出为空
        return result;
    }

    // Parse JSON output - look for "Installer" type in detects
    // 修复：MSIXBundle/MSIX 排在 MSI 前面，避免 sub-string 误匹配
    static const char* installerTypes[] = {
        "NSIS", "Inno Setup", "InstallShield",
        "Advanced Installer", "Setup Factory", "Smart Install Maker",
        "Ghost Installer", "CreateInstall", "Actual Installer",
        "SetupBuilder", "InstallAware", "Wise",
        "RAR", "7-Zip", "7z SFX", "Clickteam",
        "Paquet Builder", "AutoPlay",
        "IExpress", "WExtract", "Squirrel", "BitRock",
        "InstallScript", "WinZip",
        "APPX", "MSIXBundle", "MSIX", "WiX MSI", "WiX", "MSI"
    };

    // Find "detects" array
    size_t detectsIdx = output.find("\"detects\"");
    if (detectsIdx == std::string::npos) {
        result.errorMessage = "DIE \xe6\x9c\xaa\xe8\x83\xbd\xe8\xaf\x86\xe5\x88\xab\xe5\xae\x89\xe8\xa3\x85\xe5\x99\xa8\xe7\xb1\xbb\xe5\x9e\x8b"; // DIE 未能识别安装器类型
        return result;
    }

    std::string detectedType;
    std::string version;

    for (const char* type : installerTypes) {
        size_t idx = output.find(type, detectsIdx);
        if (idx != std::string::npos) {
            detectedType = type;
            version = ExtractVersion(output, idx);
            break;
        }
    }

    if (detectedType.empty()) {
        // Also try matching from database string signatures
        // 修复：预构建小写输出一次，避免每轮重复tolower整个字符串
        std::string lowerOutput = output;
        for (auto& c : lowerOutput) c = tolower((unsigned char)c);
        for (int i = 0; i < g_dbCount; i++) {
            const InstallerInfo& info = g_database[i];
            for (int j = 0; j < info.strSigCount; j++) {
                if (info.strSigs[j]) {
                    // Case-insensitive search in JSON output
                    std::string lowerSig = info.strSigs[j];
                    for (auto& c : lowerSig) c = tolower((unsigned char)c);
                    if (lowerOutput.find(lowerSig) != std::string::npos) {
                        detectedType = info.strSigs[j];
                        break;
                    }
                }
            }
            if (!detectedType.empty()) break;
        }
    }

    if (detectedType.empty()) {
        // 提取 DIE JSON 所有 string 值（已自带 "Name: Value" 格式），
        // 按优先级回落：Format > Installer > Packer > Protector > Tool > Library > Linker > Compiler
        std::string bestInfo;
        int bestPriority = 99;
        size_t pos = detectsIdx;
        while ((pos = output.find("\"string\"", pos)) != std::string::npos) {
            pos += 8;
            while (pos < output.size() && (output[pos] == ' ' || output[pos] == ':')) pos++;
            if (pos >= output.size() || output[pos] != '"') continue;
            pos++;
            size_t end = output.find('"', pos);
            if (end == std::string::npos) break;
            std::string val = output.substr(pos, end - pos);
            pos = end + 1;

            size_t colon = val.find(": ");
            std::string namePart = (colon != std::string::npos) ? val.substr(0, colon) : "";

            int p = 99;
            if (namePart == "Format") p = 0;
            else if (namePart == "Installer") p = 1;
            else if (namePart == "Packer") p = 2;
            else if (namePart == "Protector") p = 3;
            else if (namePart == "Tool") p = 4;
            else if (namePart == "Library") p = 5;
            else if (namePart == "Linker") p = 6;
            else if (namePart == "Compiler") p = 7;

            if (p < bestPriority) {
                bestPriority = p;
                bestInfo = val;
            }
        }

        std::string dieInfo = bestInfo;

        if (!dieInfo.empty()) {
            result.dieDetection = dieInfo;
            result.errorMessage = "未识别 (" + dieInfo + ")";
        } else
            result.errorMessage = "DIE 未能识别安装器类型";
        return result;
    }
    std::string internalType = MapDieType(detectedType);
    const InstallerInfo* dbInfo = DB_FindByType(internalType.c_str());

    if (dbInfo) {
        result.installerType = dbInfo->type;
        result.installerFullName = dbInfo->fullName;
        result.confidence = 0.85;
        result.silentCommand = ReplaceTemplate(dbInfo->cmdTemplate, GetFileName(filePath));
        for (int i = 0; i < dbInfo->paramCount; i++)
            result.params.push_back(&dbInfo->params[i]);
        result.success = true;
    } else {
        result.installerType = detectedType;
        result.installerFullName = detectedType;
        result.confidence = 0.5;
        result.success = true;
    }

    return result;
}

// ============================================================
// DetectorEngine - DIE first, PeScanner fallback
// ============================================================

ScanResult DetectorEngine::Scan(const std::wstring& filePath) {
    if (m_die.IsAvailable()) {
        ScanResult dieResult = m_die.Scan(filePath);
        if (dieResult.success)
            return dieResult;

        // DIE failed, try PeScanner
        ScanResult peResult = m_pe.Scan(filePath);
        if (peResult.success)
            return peResult;

        // Both failed, return DIE error
        dieResult.errorMessage += " (PE\xe6\x89\xab\xe6\x8f\x8f\xe4\xb9\x9f\xe6\x9c\xaa\xe5\x91\xbd\xe4\xb8\xad)"; // (PE扫描也未命中)
        return dieResult;
    }

    // DIE not available, use PeScanner only
    return m_pe.Scan(filePath);
}
