using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using SilentParamQuery.Data;

namespace SilentParamQuery.Core
{
    public class PeScanner
    {
        private const int MAX_SCAN_SIZE = 4 * 1024 * 1024; // 4MB

        public ScanResult Scan(string filePath)
        {
            var result = new ScanResult
            {
                FilePath = filePath,
                FileName = Path.GetFileName(filePath),
                DetectedBy = "PeScanner"
            };

            try
            {
                if (!File.Exists(filePath))
                {
                    result.ErrorMessage = "文件不存在";
                    return result;
                }

                byte[] fileData = ReadFileData(filePath);
                if (fileData.Length < 64)
                {
                    result.ErrorMessage = "文件过小，不是有效的PE文件";
                    return result;
                }

                // 验证MZ头
                if (fileData[0] != 0x4D || fileData[1] != 0x5A)
                {
                    // 可能是MSI文件
                    if (IsMSI(fileData))
                    {
                        return BuildMSIResult(result);
                    }
                    // 可能是APPX/MSIX/MSIXBundle (ZIP格式)
                    var zipResult = ScanZipFormat(fileData, result);
                    if (zipResult != null)
                    {
                        return zipResult;
                    }
                    result.ErrorMessage = "不是有效的PE文件 (MZ签名不匹配)";
                    return result;
                }

                // 扫描策略：魔数优先 -> 字符串扫描 -> 段名检查
                var detected = new List<Tuple<InstallerInfo, double>>();

                // 1. 检查overlay魔数
                var magicResult = ScanMagicBytes(fileData);
                if (magicResult != null)
                    detected.Add(Tuple.Create(magicResult, 0.95));

                // 2. 扫描特征字符串
                var stringResults = ScanStrings(fileData);
                foreach (var sr in stringResults)
                    detected.Add(Tuple.Create(sr.Item1, sr.Item2));

                // 3. 检查PE段名
                var sectionResult = ScanSections(fileData);
                if (sectionResult != null)
                    detected.Add(Tuple.Create(sectionResult, 0.7));

                // 4. 检查是否嵌入MSI
                if (HasEmbeddedMSI(fileData))
                {
                    var msiInfo = SilentParamDatabase.FindByType("MSIWrappedEXE");
                    if (msiInfo != null)
                        detected.Add(Tuple.Create(msiInfo, 0.6));
                }

                // Remove magic-byte-only detections: if magic matched but no string/section
                // corroborates the same type, discard to avoid false positives
                // from common byte patterns like 0xDEADBEEF
                if (magicResult != null)
                {
                    bool hasSupport = detected.Any(d => d.Item1 == magicResult && d.Item2 < 0.95);
                    if (!hasSupport)
                    {
                        detected.RemoveAll(d => d.Item1 == magicResult && d.Item2 >= 0.95);
                    }
                }

                if (detected.Count == 0)
                {
                    result.ErrorMessage = "未能识别安装器类型";
                    return result;
                }

                // 取置信度最高的结果
                var best = detected.OrderByDescending(x => x.Item2).First();
                result.InstallerType = best.Item1.Type;
                result.InstallerFullName = best.Item1.FullName;
                result.Confidence = best.Item2;
                result.Params = best.Item1.Params;
                result.SilentCommand = best.Item1.SilentCommandTemplate
                    .Replace("{file}", result.FileName);
                result.Success = true;

                return result;
            }
            catch (Exception ex)
            {
                result.ErrorMessage = $"扫描出错: {ex.Message}";
                return result;
            }
        }

        private byte[] ReadFileData(string filePath)
        {
            using (var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                int readSize = (int)Math.Min(fs.Length, MAX_SCAN_SIZE);
                byte[] data = new byte[readSize];
                fs.Read(data, 0, readSize);
                return data;
            }
        }

        private bool IsMSI(byte[] data)
        {
            if (data.Length < 8) return false;
            // OLE2 magic: D0 CF 11 E0 A1 B1 1A E1
            return data[0] == 0xD0 && data[1] == 0xCF && data[2] == 0x11 && data[3] == 0xE0
                && data[4] == 0xA1 && data[5] == 0xB1 && data[6] == 0x1A && data[7] == 0xE1;
        }

        private ScanResult BuildMSIResult(ScanResult result)
        {
            var info = SilentParamDatabase.FindByType("MSI");
            if (info != null)
            {
                result.InstallerType = info.Type;
                result.InstallerFullName = info.FullName;
                result.Confidence = 0.99;
                result.Params = info.Params;
                result.SilentCommand = info.SilentCommandTemplate
                    .Replace("{file}", result.FileName);
                result.Success = true;
            }
            return result;
        }

        private InstallerInfo ScanMagicBytes(byte[] data)
        {
            var all = SilentParamDatabase.GetAll();
            foreach (var info in all)
            {
                foreach (var magic in info.MagicBytes)
                {
                    if (magic.Length == 0) continue;
                    // 在文件末尾区域搜索魔数 (overlay)
                    int searchStart = Math.Max(0, data.Length - 1024 * 1024);
                    for (int i = searchStart; i <= data.Length - magic.Length; i++)
                    {
                        if (MatchBytes(data, i, magic))
                            return info;
                    }
                    // 也在文件开头搜索
                    for (int i = 0; i < Math.Min(data.Length, 4096) - magic.Length; i++)
                    {
                        if (MatchBytes(data, i, magic))
                            return info;
                    }
                }
            }
            return null;
        }

        private bool MatchBytes(byte[] data, int offset, byte[] pattern)
        {
            for (int i = 0; i < pattern.Length; i++)
            {
                if (data[offset + i] != pattern[i]) return false;
            }
            return true;
        }

        private List<Tuple<InstallerInfo, double>> ScanStrings(byte[] data)
        {
            var results = new List<Tuple<InstallerInfo, double>>();
            var all = SilentParamDatabase.GetAll();
            string ascii = Encoding.ASCII.GetString(data);

            foreach (var info in all)
            {
                int matchCount = 0;
                int longMatchCount = 0; // signatures >= 10 chars (high confidence)
                foreach (var sig in info.StringSignatures)
                {
                    bool found;
                    if (sig.Length < 6)
                    {
                        // Short signatures: require word boundary matching to avoid false positives
                        found = IndexOfWord(ascii, sig) >= 0;
                    }
                    else
                    {
                        found = ascii.IndexOf(sig, StringComparison.OrdinalIgnoreCase) >= 0;
                    }
                    if (found)
                    {
                        matchCount++;
                        if (sig.Length >= 10) longMatchCount++;
                    }
                }
                // Require at least 2 matches, or 1 match with a long signature (>= 10 chars)
                if (matchCount >= 2 || (matchCount >= 1 && longMatchCount >= 1))
                {
                    double confidence = Math.Min(0.9, 0.5 + matchCount * 0.15);
                    results.Add(Tuple.Create(info, confidence));
                }
            }
            return results;
        }

        // Word-boundary-aware case-insensitive substring search
        private static int IndexOfWord(string text, string pattern)
        {
            int idx = 0;
            while (idx <= text.Length - pattern.Length)
            {
                int found = text.IndexOf(pattern, idx, StringComparison.OrdinalIgnoreCase);
                if (found < 0) return -1;
                // Check word boundary before
                if (found > 0 && char.IsLetterOrDigit(text[found - 1]))
                {
                    idx = found + 1;
                    continue;
                }
                // Check word boundary after
                int end = found + pattern.Length;
                if (end < text.Length && char.IsLetterOrDigit(text[end]))
                {
                    idx = found + 1;
                    continue;
                }
                return found;
            }
            return -1;
        }

        private InstallerInfo ScanSections(byte[] data)
        {
            try
            {
                int peOffset = BitConverter.ToInt32(data, 0x3C);
                // 修复：peOffset负数校验 — 恶意PE构造负偏移导致数组越界
                if (peOffset < 0 || peOffset + 24 > data.Length) return null;
                if (data[peOffset] != 0x50 || data[peOffset + 1] != 0x45) return null; // PE

                int numSections = BitConverter.ToInt16(data, peOffset + 6);
                int optHeaderSize = BitConverter.ToInt16(data, peOffset + 20);
                int sectionStart = peOffset + 24 + optHeaderSize;

                var all = SilentParamDatabase.GetAll();
                for (int i = 0; i < numSections; i++)
                {
                    int secOffset = sectionStart + i * 40;
                    if (secOffset + 40 > data.Length) break;

                    byte[] nameBytes = new byte[8];
                    Array.Copy(data, secOffset, nameBytes, 0, 8);
                    string sectionName = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0');

                    foreach (var info in all)
                    {
                        foreach (var sn in info.SectionNames)
                        {
                            if (sectionName.Equals(sn, StringComparison.OrdinalIgnoreCase))
                                return info;
                        }
                    }
                }
            }
            // 修复：空catch — PE段解析失败(非PE文件/畸形PE)非致命，静默跳过
            catch { }
            return null;
        }

        private bool HasEmbeddedMSI(byte[] data)
        {
            // 搜索嵌入的OLE2魔数
            byte[] msiMagic = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
            for (int i = 0x1000; i < data.Length - msiMagic.Length; i++)
            {
                if (MatchBytes(data, i, msiMagic))
                    return true;
            }
            return false;
        }

        private ScanResult ScanZipFormat(byte[] data, ScanResult result)
        {
            // 检查是否为ZIP格式 (PK magic)
            if (data.Length < 4 || data[0] != 0x50 || data[1] != 0x4B || data[2] != 0x03 || data[3] != 0x04)
                return null;

            string ascii = Encoding.ASCII.GetString(data);
            var all = SilentParamDatabase.GetAll();

            // 按优先级匹配：MSIXBundle > MSIX > APPX
            var zipTypes = new[] { "MSIXBundle", "MSIX", "APPX" };
            foreach (var typeName in zipTypes)
            {
                var info = all.Find(x => x.Type == typeName);
                if (info == null) continue;

                int matchCount = 0;
                int longMatchCount = 0;
                foreach (var sig in info.StringSignatures)
                {
                    bool found = sig.Length < 6
                        ? IndexOfWord(ascii, sig) >= 0
                        : ascii.IndexOf(sig, StringComparison.OrdinalIgnoreCase) >= 0;
                    if (found)
                    {
                        matchCount++;
                        if (sig.Length >= 10) longMatchCount++;
                    }
                }
                if (matchCount >= 2 || (matchCount >= 1 && longMatchCount >= 1))
                {
                    result.InstallerType = info.Type;
                    result.InstallerFullName = info.FullName;
                    result.Confidence = Math.Min(0.95, 0.6 + matchCount * 0.15);
                    result.Params = info.Params;
                    result.SilentCommand = info.SilentCommandTemplate
                        .Replace("{file}", result.FileName);
                    result.Success = true;
                    return result;
                }
            }

            return null;
        }
    }
}
