#ifndef SCANNER_H
#define SCANNER_H

#include <windows.h>
#include <vector>
#include <string>
#include "database.h"

// Global Tools path (set by main.cpp)
void SetToolsPath(const std::wstring& path);
std::wstring GetToolsPath();

struct ScanResult {
    std::wstring filePath;
    std::wstring fileName;
    std::string installerType;
    std::string installerFullName;
    std::string silentCommand;
    std::string detectedBy;
    double confidence;
    bool success;
    std::string errorMessage;
    std::string dieDetection; // 修复：DIE检测到的非安装器信息（如".NET Framework"等）

    std::vector<const SilentParam*> params;

    ScanResult() : confidence(0), success(false) {}

    std::wstring GetConfidenceStars() const {
        if (confidence >= 0.9) return L"\x2605\x2605\x2605\x2605\x2605"; // ★★★★★
        if (confidence >= 0.7) return L"\x2605\x2605\x2605\x2605\x2606"; // ★★★★☆
        if (confidence >= 0.5) return L"\x2605\x2605\x2605\x2606\x2606"; // ★★★☆☆
        if (confidence >= 0.3) return L"\x2605\x2605\x2606\x2606\x2606"; // ★★☆☆☆
        return L"\x2605\x2606\x2606\x2606\x2606";                        // ★☆☆☆☆
    }
};

class PeScanner {
public:
    ScanResult Scan(const std::wstring& filePath);

private:
    static const int MAX_SCAN_SIZE = 4 * 1024 * 1024;

    std::vector<BYTE> ReadFileData(const std::wstring& filePath);
    bool IsMSI(const std::vector<BYTE>& data);
    bool IsZIP(const std::vector<BYTE>& data);
    ScanResult BuildMSIResult(ScanResult result);
    ScanResult ScanZipFormat(const std::vector<BYTE>& data, ScanResult result);

    const InstallerInfo* ScanMagicBytes(const std::vector<BYTE>& data);
    void ScanStrings(const std::vector<BYTE>& data, std::vector<std::pair<const InstallerInfo*, double>>& results);
    const InstallerInfo* ScanSections(const std::vector<BYTE>& data);
    bool HasEmbeddedMSI(const std::vector<BYTE>& data);

    bool MatchBytes(const std::vector<BYTE>& data, size_t offset, const BYTE* pattern, size_t patLen);
};

class DieScanner {
public:
    DieScanner();
    bool IsAvailable() const;
    ScanResult Scan(const std::wstring& filePath);

private:
    std::wstring m_diePath;
    std::string MapDieType(const std::string& dieType);
    std::string ExtractVersion(const std::string& json, size_t matchIdx);
};

class DetectorEngine {
public:
    ScanResult Scan(const std::wstring& filePath);

private:
    DieScanner m_die;
    PeScanner m_pe;
};

#endif // SCANNER_H
