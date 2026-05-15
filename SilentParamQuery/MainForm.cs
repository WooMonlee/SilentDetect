using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using SilentParamQuery.Core;
using SilentParamQuery.Data;
using SilentParamQuery.Generator;
using SilentParamQuery.Utils;

namespace SilentParamQuery
{
    public partial class MainForm : Form
    {
        private readonly DetectorEngine _engine;
        private ScanResult _currentResult;

        public MainForm()
        {
            InitializeComponent();

            // 加载窗体图标
            string iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "大内静探.ico");
            if (File.Exists(iconPath))
            {
                // 修复：Icon泄漏 — 释放旧图标后设置新图标
                if (this.Icon != null)
                {
                    this.Icon.Dispose();
                }
                this.Icon = new Icon(iconPath);
            }

            _engine = new DetectorEngine();
            this.Text = $"大内静探 - 静默参数探测器 v1.3.2 (支持 {SilentParamDatabase.Count} 种安装器)";
            UpdateDieStatus();
        }

        private void UpdateDieStatus()
        {
            if (_engine.IsDieAvailable)
                lblDieStatus.Text = $"DIE: {_engine.DiePath}";
            else
                lblDieStatus.Text = "DIE: 未找到 (使用PE扫描)";
        }

        private void lblDrop_DragEnter(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files.Length > 0 && FileHelper.IsExeFile(files[0]))
                    e.Effect = DragDropEffects.Copy;
                else
                    e.Effect = DragDropEffects.None;
            }
        }

        private void lblDrop_DragDrop(object sender, DragEventArgs e)
        {
            string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
            if (files.Length > 0)
                ScanFile(files[0]);
        }

        private void btnBrowse_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog())
            {
                dlg.Title = "选择安装文件";
                dlg.Filter = "安装包文件 (*.exe;*.msi;*.appx;*.msix;*.msixbundle)|*.exe;*.msi;*.appx;*.msix;*.msixbundle|所有文件 (*.*)|*.*";
                dlg.Multiselect = false;
                if (dlg.ShowDialog() == DialogResult.OK)
                    ScanFile(dlg.FileName);
            }
        }

        private void ScanFile(string filePath)
        {
            // 修复：DoEvents重入风险 — 扫描期间禁用输入控件
            btnBrowse.Enabled = false;
            lblDrop.AllowDrop = false;
            lblStatus.Text = "正在扫描...";
            lblFilePath.Text = filePath;
            Application.DoEvents();

            _currentResult = _engine.Scan(filePath);

            // 修复：扫描结束后恢复控件
            btnBrowse.Enabled = true;
            lblDrop.AllowDrop = true;

            if (_currentResult.Success)
            {
                lblInstallerType.Text = $"{_currentResult.InstallerFullName} ({_currentResult.InstallerType})";
                lblVersion.Text = _currentResult.Version ?? "--";
                lblDetectedBy.Text = _currentResult.DetectedBy;
                lblConfidence.Text = _currentResult.GetConfidenceStars();
                txtCommand.Text = _currentResult.SilentCommand;

                // 修复：Font泄漏 — 清空列表前释放已创建的Font对象
                foreach (ListViewItem oldItem in lstParams.Items)
                {
                    if (oldItem.Font != null && oldItem.Font != lstParams.Font)
                        oldItem.Font.Dispose();
                }
                lstParams.Items.Clear();
                foreach (var p in _currentResult.Params)
                {
                    var item = new ListViewItem(p.Switch);
                    item.SubItems.Add(p.Description);
                    if (p.IsPrimary)
                    {
                        item.Font = new System.Drawing.Font(lstParams.Font, System.Drawing.FontStyle.Bold);
                        item.ForeColor = System.Drawing.Color.FromArgb(0, 80, 160);
                    }
                    lstParams.Items.Add(item);
                }

                btnGenerateBat.Enabled = true;
                lblStatus.Text = $"扫描完成 - {_currentResult.InstallerFullName}";

                // 更新拖放区域提示
                lblDrop.Text = Path.GetFileName(filePath);
                lblDrop.ForeColor = System.Drawing.Color.FromArgb(0, 100, 0);
            }
            else
            {
                lblInstallerType.Text = "未识别";
                lblVersion.Text = "--";
                lblDetectedBy.Text = _currentResult.DetectedBy;
                lblConfidence.Text = "--";
                txtCommand.Text = "";
                // 修复：Font泄漏 — 清空列表前释放已创建的Font对象
                foreach (ListViewItem oldItem in lstParams.Items)
                {
                    if (oldItem.Font != null && oldItem.Font != lstParams.Font)
                        oldItem.Font.Dispose();
                }
                lstParams.Items.Clear();
                btnGenerateBat.Enabled = false;
                lblStatus.Text = $"扫描失败 - {_currentResult.ErrorMessage}";

                lblDrop.Text = "拖放 EXE / MSI 文件到此处";
                lblDrop.ForeColor = System.Drawing.Color.Gray;
            }
        }

        private void btnCopyCmd_Click(object sender, EventArgs e)
        {
            if (!string.IsNullOrEmpty(txtCommand.Text))
            {
                FileHelper.CopyToClipboard(txtCommand.Text);
                lblStatus.Text = "命令已复制到剪贴板";
            }
        }

        private void btnGenerateBat_Click(object sender, EventArgs e)
        {
            if (_currentResult == null || !_currentResult.Success)
                return;

            using (var dlg = new SaveFileDialog())
            {
                dlg.Title = "保存批处理文件";
                dlg.Filter = "批处理文件 (*.bat)|*.bat";
                dlg.FileName = $"silent_install_{Path.GetFileNameWithoutExtension(_currentResult.FileName)}.bat";
                dlg.DefaultExt = "bat";

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    var options = new BatchOptions
                    {
                        IncludeLog = chkLog.Checked,
                        IncludeErrorHandling = chkErrorHandling.Checked,
                        IncludeUac = chkUac.Checked,
                        IncludeComments = chkComments.Checked
                    };

                    string content = BatchGenerator.Generate(_currentResult, options);
                    BatchGenerator.SaveToFile(content, dlg.FileName);
                    lblStatus.Text = $"批处理文件已生成: {Path.GetFileName(dlg.FileName)}";

                    MessageBox.Show($"批处理文件已生成:\n{dlg.FileName}\n\n编码: ANSI (GB2312)",
                        "生成完成", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
        }
    }
}
