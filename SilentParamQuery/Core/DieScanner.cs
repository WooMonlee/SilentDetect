using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using SilentParamQuery.Data;

namespace SilentParamQuery.Core
{
    public class DieScanner
    {
        private string _diePath;

        public DieScanner()
        {
            // 优先查找程序目录下的 Tools\diec.exe
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            _diePath = Path.Combine(appDir, "Tools", "diec.exe");

            if (!File.Exists(_diePath))
            {
                // 尝试在PATH中查找
                _diePath = FindInPath("diec.exe");
            }
        }

        public bool IsAvailable
        {
            get { return !string.IsNullOrEmpty(_diePath) && File.Exists(_diePath); }
        }

        public string DiePath
        {
            get { return _diePath; }
        }

        public ScanResult Scan(string filePath)
        {
            var result = new ScanResult
            {
                FilePath = filePath,
                FileName = Path.GetFileName(filePath),
                DetectedBy = "DIE"
            };

            if (!IsAvailable)
            {
                result.ErrorMessage = "diec.exe 不可用";
                return result;
            }

            try
            {
                // 修复：命令注入 — 转义文件路径中的特殊字符（双引号 → \"  反斜杠 → \\）
                string safePath = filePath.Replace("\\", "\\\\").Replace("\"", "\\\"");

                var psi = new ProcessStartInfo
                {
                    FileName = _diePath,
                    Arguments = $"--json \"{safePath}\"",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.UTF8
                };

                string errorBuffer = ""; // 修复：收集 stderr 异步输出

                using (var proc = Process.Start(psi))
                {
                    // 修复：死锁 - 使用异步读取 stderr 避免缓冲区满死锁
                    proc.ErrorDataReceived += (s, e) => { if (e.Data != null) errorBuffer += e.Data + "\n"; };
                    proc.BeginErrorReadLine();
                    string output = proc.StandardOutput.ReadToEnd();
                    proc.WaitForExit(15000);

                    // 修复：超时未退出 — 强制杀进程
                    if (!proc.HasExited)
                    {
                        proc.Kill();
                        proc.WaitForExit(3000);
                        result.ErrorMessage = "DIE 扫描超时 (15秒)，已终止进程";
                        return result;
                    }

                    string error = errorBuffer.Trim();

                    if (proc.ExitCode != 0 && string.IsNullOrEmpty(output))
                    {
                        result.ErrorMessage = $"diec 退出码: {proc.ExitCode} - {error}";
                        return result;
                    }

                    return ParseDieOutput(result, output);
                }
            }
            catch (Exception ex)
            {
                result.ErrorMessage = $"调用 diec 出错: {ex.Message}";
                return result;
            }
        }

        private ScanResult ParseDieOutput(ScanResult result, string json)
        {
            if (string.IsNullOrWhiteSpace(json))
            {
                result.ErrorMessage = "diec 输出为空";
                return result;
            }

            try
            {
                // 简单JSON解析（避免引入Newtonsoft.Json依赖）
                // diec JSON格式: {"detects":[{"name":"NSIS","type":"installer",...},...]}
                string detectedType = null;
                string version = null;
                double confidence = 0.85;

                // 提取 detects 数组中的第一个 installer 类型
                int detectsIdx = json.IndexOf("\"detects\"");
                if (detectsIdx >= 0)
                {
                    // 查找 installer 类型
                    var installerTypes = new[] {
                        "NSIS", "Inno Setup", "InstallShield",
                        "Advanced Installer", "Setup Factory", "Smart Install Maker",
                        "Ghost Installer", "CreateInstall", "Actual Installer",
                        "SetupBuilder", "InstallAware", "Wise",
                        "RAR", "7-Zip", "7z SFX", "Clickteam",
                        "Paquet Builder", "AutoPlay",
                        "IExpress", "WExtract", "Squirrel", "BitRock",
                        "InstallScript", "WinZip",
                        "APPX", "MSIXBundle", "MSIX", "WiX MSI", "WiX", "MSI"
                        // 修复：MSIXBundle/MSIX 排在 MSI 前面，避免 IndexOf("MSI") 误匹配 "MSIX" 子串
                    };

                    foreach (var type in installerTypes)
                    {
                        int idx = json.IndexOf(type, detectsIdx, StringComparison.OrdinalIgnoreCase);
                        if (idx >= 0)
                        {
                            detectedType = type;
                            // 尝试提取版本号
                            version = ExtractVersion(json, idx);
                            break;
                        }
                    }
                }

                if (detectedType == null)
                {
                    // 尝试从全文匹配
                    var all = SilentParamDatabase.GetAll();
                    foreach (var info in all)
                    {
                        foreach (var sig in info.StringSignatures)
                        {
                            if (json.IndexOf(sig, StringComparison.OrdinalIgnoreCase) >= 0)
                            {
                                detectedType = info.Type;
                                confidence = 0.7;
                                break;
                            }
                        }
                        if (detectedType != null) break;
                    }
                }

                if (detectedType == null)
                {
                    result.ErrorMessage = "DIE 未能识别安装器类型";
                    return result;
                }

                // 映射到内部类型
                string internalType = MapDieTypeToInternal(detectedType);
                var dbInfo = SilentParamDatabase.FindByType(internalType);

                if (dbInfo != null)
                {
                    result.InstallerType = dbInfo.Type;
                    result.InstallerFullName = dbInfo.FullName;
                    result.Version = version;
                    result.Confidence = confidence;
                    result.Params = dbInfo.Params;
                    result.SilentCommand = dbInfo.SilentCommandTemplate
                        .Replace("{file}", result.FileName);
                    result.Success = true;
                }
                else
                {
                    result.InstallerType = detectedType;
                    result.InstallerFullName = detectedType;
                    result.Version = version;
                    result.Confidence = 0.5;
                    result.Success = true;
                }

                return result;
            }
            catch (Exception ex)
            {
                result.ErrorMessage = $"解析DIE输出出错: {ex.Message}";
                return result;
            }
        }

        private string ExtractVersion(string json, int matchIdx)
        {
            try
            {
                // 在匹配位置附近查找版本模式 (X.X.X 或 X.X)
                int searchStart = Math.Max(0, matchIdx - 10);
                int searchEnd = Math.Min(json.Length, matchIdx + 100);
                string segment = json.Substring(searchStart, searchEnd - searchStart);

                for (int i = 0; i < segment.Length - 2; i++)
                {
                    if (char.IsDigit(segment[i]) && i + 1 < segment.Length && segment[i + 1] == '.')
                    {
                        int start = i;
                        int end = i;
                        while (end < segment.Length && (char.IsDigit(segment[end]) || segment[end] == '.'))
                            end++;
                        string ver = segment.Substring(start, end - start);
                        if (ver.Length >= 3 && ver.Contains("."))
                            return ver;
                    }
                }
            }
            catch
            {
                // 修复：空catch — 版本提取失败属正常情况(无版本信息)，静默跳过
            }
            return null;
        }

        private string MapDieTypeToInternal(string dieType)
        {
            var mapping = new System.Collections.Generic.Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                { "NSIS", "NSIS" },
                { "Inno Setup", "InnoSetup" },
                { "InstallShield", "InstallShield" },
                { "MSI", "MSI" },
                { "WiX", "WiXBurn" },
                { "Advanced Installer", "AdvancedInstaller" },
                { "Setup Factory", "SetupFactory" },
                { "Smart Install Maker", "SmartInstallMaker" },
                { "Ghost Installer", "GhostInstaller" },
                { "CreateInstall", "CreateInstall" },
                { "Actual Installer", "ActualInstaller" },
                { "SetupBuilder", "SetupBuilder" },
                { "InstallAware", "InstallAware" },
                { "Wise", "WiseInstaller" },
                { "RAR", "RARSFX" },
                { "7-Zip", "7ZipSFX" },
                { "7z SFX", "7ZipSFX" },
                { "Clickteam", "ClickteamIC" },
                { "Paquet Builder", "PaquetBuilder" },
                { "AutoPlay", "AutoPlayMS" },
                { "IExpress", "IExpress" },
                { "WExtract", "IExpress" },
                { "Squirrel", "Squirrel" },
                { "BitRock", "BitRockInstallBuilder" },
                { "InstallScript", "InstallScript" },
                { "WinZip", "WinZipSFX" },
                { "WiX MSI", "WiXMSI" },
                { "APPX", "APPX" },
                { "MSIX", "MSIX" },
                { "MSIXBundle", "MSIXBundle" }
            };

            string result;
            if (mapping.TryGetValue(dieType, out result))
                return result;
            return dieType;
        }

        private string FindInPath(string fileName)
        {
            string pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (string.IsNullOrEmpty(pathEnv)) return null;

            foreach (var dir in pathEnv.Split(';'))
            {
                try
                {
                    string fullPath = Path.Combine(dir.Trim(), fileName);
                    if (File.Exists(fullPath))
                        return fullPath;
                }
                catch
                {
                    // 修复：空catch — PATH 中个别无效目录跳过即可，不影响查找
                }
            }
            return null;
        }
    }
}
