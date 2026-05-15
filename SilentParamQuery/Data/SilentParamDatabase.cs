using System.Collections.Generic;
using SilentParamQuery.Core;

namespace SilentParamQuery.Data
{
    public class InstallerInfo
    {
        public string Type { get; set; }
        public string FullName { get; set; }
        public List<SilentParam> Params { get; set; }
        public string SilentCommandTemplate { get; set; }
        public List<string> StringSignatures { get; set; }
        public List<byte[]> MagicBytes { get; set; }
        public List<string> SectionNames { get; set; }

        public InstallerInfo()
        {
            Params = new List<SilentParam>();
            StringSignatures = new List<string>();
            MagicBytes = new List<byte[]>();
            SectionNames = new List<string>();
        }
    }

    public static class SilentParamDatabase
    {
        private static List<InstallerInfo> _installers;

        public static List<InstallerInfo> GetAll()
        {
            if (_installers != null) return _installers;
            _installers = new List<InstallerInfo>();
            InitDatabase();
            return _installers;
        }

        public static int Count
        {
            get { return GetAll().Count; }
        }

        public static InstallerInfo FindByType(string type)
        {
            return GetAll().Find(x => x.Type == type);
        }

        private static void InitDatabase()
        {
            // 1. NSIS
            _installers.Add(new InstallerInfo
            {
                Type = "NSIS",
                FullName = "Nullsoft Scriptable Install System",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默安装 (必需，大写)", true),
                    new SilentParam("/D=<path>", "指定安装目录 (必须为最后一个参数)"),
                    new SilentParam("/NCRC", "跳过CRC完整性校验"),
                    new SilentParam("/NOCD", "禁用当前目录更改")
                },
                StringSignatures = new List<string> { "NullsoftInst", "NSIS Error", "!@Install@!UTF-8!", "NSIS_INSTALLDIR" },
                MagicBytes = new List<byte[]> { new byte[] { 0xEF, 0xBE, 0xAD, 0xDE } },
                SectionNames = new List<string> { ".ndata", ".nsis" }
            });

            // 2. Inno Setup
            _installers.Add(new InstallerInfo
            {
                Type = "InnoSetup",
                FullName = "Inno Setup",
                SilentCommandTemplate = "\"{file}\" /VERYSILENT /SP-",
                Params = new List<SilentParam>
                {
                    new SilentParam("/VERYSILENT", "完全静默 (推荐)", true),
                    new SilentParam("/SILENT", "静默模式 (显示进度条)"),
                    new SilentParam("/SUPPRESSMSGBOXES", "抑制所有消息框"),
                    new SilentParam("/SP-", "禁用初始安装提示"),
                    new SilentParam("/NORESTART", "禁止重启提示"),
                    new SilentParam("/NOCANCEL", "禁止取消安装"),
                    new SilentParam("/DIR=\"x:\\path\"", "指定安装目录"),
                    new SilentParam("/GROUP=\"folder\"", "设置开始菜单文件夹"),
                    new SilentParam("/NOICONS", "不创建开始菜单图标"),
                    new SilentParam("/LOADINF=\"file\"", "从INF文件加载设置"),
                    new SilentParam("/SAVEINF=\"file\"", "保存设置到INF文件"),
                    new SilentParam("/LANG=language", "设置安装语言"),
                    new SilentParam("/COMPONENTS=\"c1,c2\"", "选择安装组件"),
                    new SilentParam("/TASKS=\"t1,t2\"", "选择执行任务"),
                    new SilentParam("/PASSWORD=pwd", "提供加密安装包密码"),
                    new SilentParam("/LOG=\"file\"", "启用安装日志")
                },
                StringSignatures = new List<string> { "Inno Setup Setup Data", "JR.Inno.Setup", "Inno Setup", "InnoSetup:" },
                MagicBytes = new List<byte[]> { new byte[] { 0x72, 0x44, 0x6C, 0x50, 0x74, 0x53 } }, // rDlPtS
                SectionNames = new List<string> { ".itext" }
            });

            // 3. InstallShield
            _installers.Add(new InstallerInfo
            {
                Type = "InstallShield",
                FullName = "InstallShield",
                SilentCommandTemplate = "\"{file}\" /s /v\"/qn\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("/s", "静默模式 (必需，小写)", true),
                    new SilentParam("/v\"/qn\"", "传递静默参数给MSI引擎"),
                    new SilentParam("/v\"/qb\"", "基本UI (仅进度条)"),
                    new SilentParam("/r", "录制响应文件 (.iss)"),
                    new SilentParam("/r /f1\"path.iss\"", "录制响应文件到指定路径"),
                    new SilentParam("/f1\"path.iss\"", "使用指定响应文件"),
                    new SilentParam("/f2\"path.log\"", "指定日志文件路径"),
                    new SilentParam("/m\"file.iss\"", "维护模式的ISS文件"),
                    new SilentParam("/uninst", "卸载模式")
                },
                StringSignatures = new List<string> { "InstallShield", "InstallShield Wizard", "ISSETUP.DLL", "_setup.dll", "InstallShield Setup" },
                MagicBytes = new List<byte[]> { new byte[] { 0x49, 0x53, 0x63, 0x34 }, new byte[] { 0x49, 0x53, 0x63, 0x35 }, new byte[] { 0x49, 0x53, 0x63, 0x36 } }, // ISc4/5/6
                SectionNames = new List<string> { ".isg", ".ISG" }
            });

            // 4. Windows Installer (MSI)
            _installers.Add(new InstallerInfo
            {
                Type = "MSI",
                FullName = "Windows Installer (MSI)",
                SilentCommandTemplate = "msiexec /i \"{file}\" /qn /norestart",
                Params = new List<SilentParam>
                {
                    new SilentParam("msiexec /i", "安装 (必需前缀)", true),
                    new SilentParam("/qn", "完全静默，无UI"),
                    new SilentParam("/qb", "基本UI (仅进度条)"),
                    new SilentParam("/qb!", "基本UI，无取消按钮"),
                    new SilentParam("/passive", "被动模式 (进度条，无交互)"),
                    new SilentParam("/quiet", "静默模式 (等同/qn)"),
                    new SilentParam("/norestart", "禁止重启"),
                    new SilentParam("/forcerestart", "安装后强制重启"),
                    new SilentParam("/l*v \"file.log\"", "详细日志记录"),
                    new SilentParam("ALLUSERS=1", "所有用户安装 (计算机级)"),
                    new SilentParam("REBOOT=ReallySuppress", "抑制重启"),
                    new SilentParam("INSTALLDIR=\"C:\\Path\"", "指定安装目录"),
                    new SilentParam("ADDLOCAL=ALL", "安装所有功能"),
                    new SilentParam("TRANSFORMS=\"custom.mst\"", "应用转换文件")
                },
                StringSignatures = new List<string> { "Windows Installer", "msiexec" },
                MagicBytes = new List<byte[]> { new byte[] { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 } },
                SectionNames = new List<string>()
            });

            // 5. WiX Burn
            _installers.Add(new InstallerInfo
            {
                Type = "WiXBurn",
                FullName = "WiX Burn Bundle",
                SilentCommandTemplate = "\"{file}\" /quiet /norestart",
                Params = new List<SilentParam>
                {
                    new SilentParam("/quiet", "完全静默 (必需)", true),
                    new SilentParam("/passive", "被动模式 (仅进度条)"),
                    new SilentParam("/norestart", "禁止重启"),
                    new SilentParam("/log \"file.log\"", "启用日志"),
                    new SilentParam("/layout \"path\"", "下载包到指定路径"),
                    new SilentParam("/uninstall", "卸载"),
                    new SilentParam("/repair", "修复安装")
                },
                StringSignatures = new List<string> { "BootstrapperCore.dll", "WixBundle", "WixStandardBootstrapperApplication", "WixStdBA" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 6. Advanced Installer
            _installers.Add(new InstallerInfo
            {
                Type = "AdvancedInstaller",
                FullName = "Advanced Installer (Caphyon)",
                SilentCommandTemplate = "\"{file}\" /q",
                Params = new List<SilentParam>
                {
                    new SilentParam("/q", "静默模式 (必需)", true),
                    new SilentParam("/quiet", "静默模式 (备用)"),
                    new SilentParam("/qb", "基本UI (进度条)"),
                    new SilentParam("/qr", "精简UI"),
                    new SilentParam("/passive", "被动模式"),
                    new SilentParam("/norestart", "禁止重启"),
                    new SilentParam("/L*V \"file.log\"", "详细日志"),
                    new SilentParam("REBOOT=ReallySuppress", "抑制重启"),
                    new SilentParam("INSTALLDIR=\"C:\\Path\"", "指定安装目录")
                },
                StringSignatures = new List<string> { "AdvancedInstaller", "Advanced Installer", "Caphyon", "AI_SETUP", "advancedinstaller.com" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 7. Setup Factory
            _installers.Add(new InstallerInfo
            {
                Type = "SetupFactory",
                FullName = "Setup Factory (Indigo Rose)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式 (必需，大写)", true),
                    new SilentParam("/s", "静默模式 (小写)"),
                    new SilentParam("/D=<path>", "指定安装目录"),
                    new SilentParam("/NOCANCEL", "禁用取消按钮")
                },
                StringSignatures = new List<string> { "SetupFactory", "Setup Factory", "Indigo Rose", "AFData", "SUF_", "irsetup" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 8. Smart Install Maker
            _installers.Add(new InstallerInfo
            {
                Type = "SmartInstallMaker",
                FullName = "Smart Install Maker (InstallBuilders)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式", true),
                    new SilentParam("/silent", "静默模式 (备用)"),
                    new SilentParam("/SILENT", "静默模式 (大写)"),
                    new SilentParam("/VERYSILENT", "完全静默"),
                    new SilentParam("/DIR=\"C:\\Path\"", "指定安装目录"),
                    new SilentParam("/NORESTART", "禁止重启")
                },
                StringSignatures = new List<string> { "Smart Install Maker", "InstallBuilders", "smartinstallmaker" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 9. Ghost Installer
            _installers.Add(new InstallerInfo
            {
                Type = "GhostInstaller",
                FullName = "Ghost Installer (Extreme Internet)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式", true),
                    new SilentParam("/verysilent", "完全静默"),
                    new SilentParam("/sp-", "禁用初始提示"),
                    new SilentParam("/norestart", "禁止重启"),
                    new SilentParam("/dir=\"C:\\Path\"", "指定安装目录")
                },
                StringSignatures = new List<string> { "GHOST", "Ghost Installer", "GHOST Installer Studio", "Extreme Internet", "ginstall", "ghostinstall" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 10. CreateInstall
            _installers.Add(new InstallerInfo
            {
                Type = "CreateInstall",
                FullName = "CreateInstall (Gentee)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式 (必需)", true),
                    new SilentParam("/s", "静默模式 (小写)"),
                    new SilentParam("/D=<path>", "指定安装目录"),
                    new SilentParam("/NOCANCEL", "禁用取消按钮"),
                    new SilentParam("/NCRC", "跳过CRC校验")
                },
                StringSignatures = new List<string> { "CreateInstall", "createinstall.com" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 11. Actual Installer
            _installers.Add(new InstallerInfo
            {
                Type = "ActualInstaller",
                FullName = "Actual Installer (Softeza)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式 (必需，大写)", true),
                    new SilentParam("/SILENT", "静默模式 (备用)"),
                    new SilentParam("/D=<path>", "指定安装目录 (必须为最后一个参数)"),
                    new SilentParam("/NOCANCEL", "禁用取消按钮")
                },
                StringSignatures = new List<string> { "Actual Installer", "Softeza", "actualinstaller" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 12. SetupBuilder
            _installers.Add(new InstallerInfo
            {
                Type = "SetupBuilder",
                FullName = "SetupBuilder (Lindersoft)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默模式", true),
                    new SilentParam("/Q", "安静模式"),
                    new SilentParam("/SILENT", "静默模式"),
                    new SilentParam("/VERYSILENT", "完全静默"),
                    new SilentParam("/SP-", "禁用初始提示"),
                    new SilentParam("/NORESTART", "禁止重启"),
                    new SilentParam("/SUPPRESSMSGBOXES", "抑制消息框"),
                    new SilentParam("/DIR=\"x:\\path\"", "指定安装目录"),
                    new SilentParam("/NOICONS", "不创建图标"),
                    new SilentParam("/LOG=\"file.log\"", "安装日志"),
                    new SilentParam("/SAVETOSILENT", "生成静默安装配置文件")
                },
                StringSignatures = new List<string> { "SetupBuilder", "Lindersoft", "sbsetup", "setupbuilder" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 13. InstallAware
            _installers.Add(new InstallerInfo
            {
                Type = "InstallAware",
                FullName = "InstallAware",
                SilentCommandTemplate = "\"{file}\" /s /v\"/qn\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("/s", "静默模式 (必需)", true),
                    new SilentParam("/v\"/qn\"", "传递MSI静默参数"),
                    new SilentParam("/v\"/l*v file.log\"", "详细日志"),
                    new SilentParam("/v\"REBOOT=ReallySuppress\"", "抑制重启"),
                    new SilentParam("/x", "静默卸载")
                },
                StringSignatures = new List<string> { "InstallAware", "installaware.com", "MsiAware" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 14. Wise Installer
            _installers.Add(new InstallerInfo
            {
                Type = "WiseInstaller",
                FullName = "Wise Installer (Wise Solutions)",
                SilentCommandTemplate = "\"{file}\" /s",
                Params = new List<SilentParam>
                {
                    new SilentParam("/s", "静默模式", true),
                    new SilentParam("/S", "静默模式 (大写)"),
                    new SilentParam("/q", "安静模式"),
                    new SilentParam("/qn", "安静，无UI"),
                    new SilentParam("/s /v\"/qn\"", "传递MSI静默参数"),
                    new SilentParam("/x", "卸载")
                },
                StringSignatures = new List<string> { "WiseMain", "WiseScript", "__WISE__", "Wise Solutions", "Wise Installation" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 15. RAR SFX
            _installers.Add(new InstallerInfo
            {
                Type = "RARSFX",
                FullName = "RAR 自解压档案",
                SilentCommandTemplate = "\"{file}\" -s -o+",
                Params = new List<SilentParam>
                {
                    new SilentParam("-s", "静默模式 (抑制所有提示)", true),
                    new SilentParam("-s1", "仅隐藏解压对话框"),
                    new SilentParam("-s2", "隐藏所有对话框"),
                    new SilentParam("-d<path>", "指定目标目录"),
                    new SilentParam("-o+", "覆盖已有文件"),
                    new SilentParam("-o-", "不覆盖已有文件"),
                    new SilentParam("-p<password>", "提供加密密码"),
                    new SilentParam("-ep1", "排除基础目录")
                },
                StringSignatures = new List<string> { "Rar!", "WinRAR", "RAR SFX" },
                MagicBytes = new List<byte[]> { new byte[] { 0x52, 0x61, 0x72, 0x21 } }, // Rar!
                SectionNames = new List<string>()
            });

            // 16. 7-Zip SFX
            _installers.Add(new InstallerInfo
            {
                Type = "7ZipSFX",
                FullName = "7-Zip 自解压档案",
                SilentCommandTemplate = "\"{file}\" -y -aoa",
                Params = new List<SilentParam>
                {
                    new SilentParam("-y", "对所有查询假设为是", true),
                    new SilentParam("-aoa", "覆盖所有已有文件"),
                    new SilentParam("-aos", "跳过已有文件"),
                    new SilentParam("-aou", "自动重命名解压文件"),
                    new SilentParam("-o<path>", "指定输出目录"),
                    new SilentParam("-bso0", "抑制标准输出"),
                    new SilentParam("-bsp0", "抑制进度输出"),
                    new SilentParam("-p<password>", "提供密码")
                },
                StringSignatures = new List<string> { "7-Zip", "7zSFX", "7zS2", "Igor Pavlov" },
                MagicBytes = new List<byte[]> { new byte[] { 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C } },
                SectionNames = new List<string>()
            });

            // 17. Clickteam Install Creator
            _installers.Add(new InstallerInfo
            {
                Type = "ClickteamIC",
                FullName = "Clickteam Install Creator",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默安装", true),
                    new SilentParam("/SILENT", "静默模式 (备用)"),
                    new SilentParam("/D=<path>", "指定安装目录"),
                    new SilentParam("/NORESTART", "禁止重启"),
                    new SilentParam("/VERYSILENT", "完全静默"),
                    new SilentParam("/SUPPRESSMSGBOXES", "抑制消息框")
                },
                StringSignatures = new List<string> { "Clickteam", "Install Creator", "InstallCreator" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 18. Paquet Builder
            _installers.Add(new InstallerInfo
            {
                Type = "PaquetBuilder",
                FullName = "Paquet Builder (GDG Soft)",
                SilentCommandTemplate = "\"{file}\" /SILENT",
                Params = new List<SilentParam>
                {
                    new SilentParam("/SILENT", "静默模式", true),
                    new SilentParam("/VERYSILENT", "完全静默"),
                    new SilentParam("/S", "静默模式 (备用)"),
                    new SilentParam("/q", "安静模式"),
                    new SilentParam("/quiet", "安静模式")
                },
                StringSignatures = new List<string> { "Paquet Builder", "GDG Soft", "GDGSoft", "paquetbuilder" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 19. AutoPlay Media Studio
            _installers.Add(new InstallerInfo
            {
                Type = "AutoPlayMS",
                FullName = "AutoPlay Media Studio (Indigo Rose)",
                SilentCommandTemplate = "\"{file}\" /S",
                Params = new List<SilentParam>
                {
                    new SilentParam("/S", "静默安装", true),
                    new SilentParam("/S /v\"/qn\"", "静默 + MSI /qn"),
                    new SilentParam("/S /v\"ALLUSERS=1 /qn\"", "所有用户静默安装")
                },
                StringSignatures = new List<string> { "AutoPlay Media Studio", "Indigo Rose", "IR_SETUP" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 20. Generic MSI Wrapped EXE
            _installers.Add(new InstallerInfo
            {
                Type = "MSIWrappedEXE",
                FullName = "通用MSI封装EXE",
                SilentCommandTemplate = "\"{file}\" /qn /norestart",
                Params = new List<SilentParam>
                {
                    new SilentParam("/qn /norestart", "MSI静默参数 (尝试)", true),
                    new SilentParam("/quiet /norestart", "静默参数 (备用)"),
                    new SilentParam("/s /v\"/qn\"", "InstallShield封装格式"),
                    new SilentParam("/q", "Advanced Installer格式"),
                    new SilentParam("msiexec /i extracted.msi /qn", "先解压MSI再静默安装")
                },
                StringSignatures = new List<string> { "msiexec", "Windows Installer" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 21. IExpress / WExtract
            _installers.Add(new InstallerInfo
            {
                Type = "IExpress",
                FullName = "IExpress / WExtract (Windows自解压)",
                SilentCommandTemplate = "\"{file}\" /Q",
                Params = new List<SilentParam>
                {
                    new SilentParam("/Q", "静默模式 (抑制所有UI)", true),
                    new SilentParam("/Q:u", "用户级静默 (仅抑制用户UI)"),
                    new SilentParam("/Q:a", "管理员级静默 (抑制所有UI)"),
                    new SilentParam("/T:<fullpath>", "指定解压目录"),
                    new SilentParam("/C:<cmd>", "指定解压后执行的命令"),
                    new SilentParam("/C:\"cmd args\"", "带参数的解压后命令"),
                    new SilentParam("/R:n", "安装后不重启"),
                    new SilentParam("/R:s", "安装后强制重启"),
                    new SilentParam("/D:<path>", "指定默认解压路径")
                },
                StringSignatures = new List<string> { "WExtract", "IExpress", "wextract.exe", "Microsoft Self-Extractor", "CABINET" },
                MagicBytes = new List<byte[]> { new byte[] { 0x49, 0x53, 0x63, 0x28 } }, // ISc( variant
                SectionNames = new List<string>()
            });

            // 22. Squirrel (Electron Apps)
            _installers.Add(new InstallerInfo
            {
                Type = "Squirrel",
                FullName = "Squirrel.Windows (Electron应用)",
                SilentCommandTemplate = "\"{file}\" --silent",
                Params = new List<SilentParam>
                {
                    new SilentParam("--silent", "静默安装 (必需)", true),
                    new SilentParam("--force-run", "安装后立即启动应用"),
                    new SilentParam("--createShortcut", "创建桌面快捷方式"),
                    new SilentParam("--no-createShortcut", "不创建快捷方式"),
                    new SilentParam("--shortcut-locations=desktop,startmenu", "指定快捷方式位置"),
                    new SilentParam("--update-only", "仅更新已安装版本")
                },
                StringSignatures = new List<string> { "Squirrel", "SquirrelSetup", "SquirrelUpdate", "nupkg", "NuGet", "electron-builder" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 23. BitRock InstallBuilder
            _installers.Add(new InstallerInfo
            {
                Type = "BitRockInstallBuilder",
                FullName = "BitRock InstallBuilder",
                SilentCommandTemplate = "\"{file}\" --mode unattended",
                Params = new List<SilentParam>
                {
                    new SilentParam("--mode unattended", "完全静默 (必需)", true),
                    new SilentParam("--mode text", "文本模式 (命令行交互)"),
                    new SilentParam("--unattendedmodeui none", "无UI静默"),
                    new SilentParam("--unattendedmodeui minimal", "最小化UI"),
                    new SilentParam("--prefix \"C:\\Path\"", "指定安装目录"),
                    new SilentParam("--installer-language en", "设置安装语言"),
                    new SilentParam("--optionfile \"file.txt\"", "使用选项文件"),
                    new SilentParam("--debuglevel 4", "调试级别 (日志详细度)"),
                    new SilentParam("--enable-components comp1", "启用指定组件"),
                    new SilentParam("--disable-components comp1", "禁用指定组件")
                },
                StringSignatures = new List<string> { "BitRock InstallBuilder", "installbuilder", "BitRock", "VMware InstallBuilder" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 24. InstallScript (InstallShield脚本模式)
            _installers.Add(new InstallerInfo
            {
                Type = "InstallScript",
                FullName = "InstallShield InstallScript",
                SilentCommandTemplate = "\"{file}\" /s /f1\"setup.iss\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("/s", "静默模式 (必需)", true),
                    new SilentParam("/f1\"path\\setup.iss\"", "指定ISS响应文件路径"),
                    new SilentParam("/f2\"path\\setup.log\"", "指定日志文件路径"),
                    new SilentParam("/r", "录制模式 (生成ISS响应文件)"),
                    new SilentParam("/r /f1\"path\\setup.iss\"", "录制到指定路径"),
                    new SilentParam("/z\"param=value\"", "传递自定义参数"),
                    new SilentParam("/x", "静默卸载"),
                    new SilentParam("/m\"productname\"", "维护模式"),
                    new SilentParam("/uninst", "启动卸载程序")
                },
                StringSignatures = new List<string> { "InstallScript", "setup.iss", "InstallShield Script", "IScriptModule", "isrt.dll", "isbew.exe" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 25. WinZip Self-Extractor
            _installers.Add(new InstallerInfo
            {
                Type = "WinZipSFX",
                FullName = "WinZip 自解压档案",
                SilentCommandTemplate = "\"{file}\" -auto -o \"C:\\Path\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("-auto", "自动解压 (无对话框)", true),
                    new SilentParam("-o", "解压到指定目录 (后跟路径)"),
                    new SilentParam("-y", "覆盖已有文件时不提示"),
                    new SilentParam("-inst", "解压后运行安装命令"),
                    new SilentParam("-j", "解压时不包含路径信息"),
                    new SilentParam("-d<path>", "指定目标目录"),
                    new SilentParam("-np", "不显示进度窗口")
                },
                StringSignatures = new List<string> { "WinZip", "WinZip Self-Extractor", "PKWARE", "WinZipSFX", "self-extractor" },
                MagicBytes = new List<byte[]>(),
                SectionNames = new List<string>()
            });

            // 26. WiX MSI (WiX编译的MSI包)
            _installers.Add(new InstallerInfo
            {
                Type = "WiXMSI",
                FullName = "WiX Toolset MSI包",
                SilentCommandTemplate = "msiexec /i \"{file}\" /qn /norestart",
                Params = new List<SilentParam>
                {
                    new SilentParam("msiexec /i", "安装 (必需前缀)", true),
                    new SilentParam("/qn", "完全静默，无UI"),
                    new SilentParam("/qb", "基本UI (仅进度条)"),
                    new SilentParam("/passive", "被动模式 (进度条无交互)"),
                    new SilentParam("/norestart", "禁止重启"),
                    new SilentParam("/l*v \"file.log\"", "详细日志记录"),
                    new SilentParam("ALLUSERS=1", "所有用户安装"),
                    new SilentParam("REBOOT=ReallySuppress", "抑制重启"),
                    new SilentParam("ADDLOCAL=ALL", "安装所有功能"),
                    new SilentParam("MSIFASTINSTALL=7", "快速安装优化")
                },
                StringSignatures = new List<string> { "WixUI", "WiX", "wixlib", "WiX Toolset", "MSI498", "WindowsInstaller" },
                MagicBytes = new List<byte[]> { new byte[] { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 } },
                SectionNames = new List<string>()
            });

            // 27. APPX (Windows应用包)
            _installers.Add(new InstallerInfo
            {
                Type = "APPX",
                FullName = "APPX (Windows应用包)",
                SilentCommandTemplate = "powershell -Command \"Add-AppxPackage -Path '{file}'\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("Add-AppxPackage -Path", "安装应用包 (必需)", true),
                    new SilentParam("-DependencyPath \"dep.appx\"", "指定依赖包路径"),
                    new SilentParam("-ForceApplicationShutdown", "强制关闭运行中的应用"),
                    new SilentParam("-ForceUpdateFromAnyVersion", "强制覆盖任意版本"),
                    new SilentParam("-Update", "更新已安装的应用"),
                    new SilentParam("-AllowUnsigned", "允许安装未签名包"),
                    new SilentParam("-ExternalLocation \"path\"", "指定外部位置")
                },
                StringSignatures = new List<string> { "AppxManifest", "AppxBlockMap", "AppxSignature", "windows.com/appx" },
                MagicBytes = new List<byte[]> { new byte[] { 0x50, 0x4B, 0x03, 0x04 } }, // ZIP magic (PK)
                SectionNames = new List<string>()
            });

            // 28. MSIX (现代Windows应用包)
            _installers.Add(new InstallerInfo
            {
                Type = "MSIX",
                FullName = "MSIX (现代Windows应用包)",
                SilentCommandTemplate = "powershell -Command \"Add-AppxPackage -Path '{file}'\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("Add-AppxPackage -Path", "安装应用包 (必需)", true),
                    new SilentParam("-DependencyPath \"dep.msix\"", "指定依赖包路径"),
                    new SilentParam("-ForceApplicationShutdown", "强制关闭运行中的应用"),
                    new SilentParam("-ForceUpdateFromAnyVersion", "强制覆盖任意版本"),
                    new SilentParam("-Update", "更新已安装的应用"),
                    new SilentParam("-AllowUnsigned", "允许安装未签名包"),
                    new SilentParam("-ExternalLocation \"path\"", "指定外部位置"),
                    new SilentParam("-VolumePath \"path\"", "指定安装卷路径")
                },
                StringSignatures = new List<string> { "AppxManifest", "MSIX", "msix", "windows.com/msix" },
                MagicBytes = new List<byte[]> { new byte[] { 0x50, 0x4B, 0x03, 0x04 } }, // ZIP magic (PK)
                SectionNames = new List<string>()
            });

            // 29. MSIXBundle (MSIX捆绑包)
            _installers.Add(new InstallerInfo
            {
                Type = "MSIXBundle",
                FullName = "MSIXBundle (应用捆绑包)",
                SilentCommandTemplate = "powershell -Command \"Add-AppxPackage -Path '{file}'\"",
                Params = new List<SilentParam>
                {
                    new SilentParam("Add-AppxPackage -Path", "安装捆绑包 (必需)", true),
                    new SilentParam("-DependencyPath \"dep.msix\"", "指定依赖包路径"),
                    new SilentParam("-ForceApplicationShutdown", "强制关闭运行中的应用"),
                    new SilentParam("-ForceUpdateFromAnyVersion", "强制覆盖任意版本"),
                    new SilentParam("-Update", "更新已安装的应用"),
                    new SilentParam("-AllowUnsigned", "允许安装未签名包")
                },
                StringSignatures = new List<string> { "AppxBundleManifest", "MSIXBundle", "msixbundle", "windows.com/appxbundle" },
                MagicBytes = new List<byte[]> { new byte[] { 0x50, 0x4B, 0x03, 0x04 } }, // ZIP magic (PK)
                SectionNames = new List<string>()
            });
        }
    }
}
