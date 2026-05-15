namespace SilentParamQuery
{
    partial class MainForm
    {
        // 修复：移除无用components字段(始终为null，冗余代码)
        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.grpDrop = new System.Windows.Forms.GroupBox();
            this.lblDrop = new System.Windows.Forms.Label();
            this.btnBrowse = new System.Windows.Forms.Button();
            this.lblFilePath = new System.Windows.Forms.Label();
            this.grpResult = new System.Windows.Forms.GroupBox();
            this.lblConfidence = new System.Windows.Forms.Label();
            this.lblDetectedBy = new System.Windows.Forms.Label();
            this.lblVersion = new System.Windows.Forms.Label();
            this.lblInstallerType = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.grpParams = new System.Windows.Forms.GroupBox();
            this.lstParams = new System.Windows.Forms.ListView();
            this.colSwitch = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.colDesc = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.grpCommand = new System.Windows.Forms.GroupBox();
            this.txtCommand = new System.Windows.Forms.TextBox();
            this.btnCopyCmd = new System.Windows.Forms.Button();
            this.grpBatch = new System.Windows.Forms.GroupBox();
            this.chkComments = new System.Windows.Forms.CheckBox();
            this.chkUac = new System.Windows.Forms.CheckBox();
            this.chkErrorHandling = new System.Windows.Forms.CheckBox();
            this.chkLog = new System.Windows.Forms.CheckBox();
            this.btnGenerateBat = new System.Windows.Forms.Button();
            this.statusStrip = new System.Windows.Forms.StatusStrip();
            this.lblStatus = new System.Windows.Forms.ToolStripStatusLabel();
            this.lblDieStatus = new System.Windows.Forms.ToolStripStatusLabel();
            this.grpDrop.SuspendLayout();
            this.grpResult.SuspendLayout();
            this.grpParams.SuspendLayout();
            this.grpCommand.SuspendLayout();
            this.grpBatch.SuspendLayout();
            this.statusStrip.SuspendLayout();
            this.SuspendLayout();
            //
            // grpDrop
            //
            this.grpDrop.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.grpDrop.Controls.Add(this.lblDrop);
            this.grpDrop.Controls.Add(this.btnBrowse);
            this.grpDrop.Controls.Add(this.lblFilePath);
            this.grpDrop.Location = new System.Drawing.Point(12, 12);
            this.grpDrop.Name = "grpDrop";
            this.grpDrop.Size = new System.Drawing.Size(560, 100);
            this.grpDrop.TabIndex = 0;
            this.grpDrop.TabStop = false;
            this.grpDrop.Text = "文件选择";
            //
            // lblDrop
            //
            this.lblDrop.AllowDrop = true;
            this.lblDrop.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.lblDrop.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(240)))), ((int)(((byte)(248)))), ((int)(((byte)(255)))));
            this.lblDrop.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.lblDrop.Font = new System.Drawing.Font("Microsoft YaHei UI", 11F);
            this.lblDrop.ForeColor = System.Drawing.Color.Gray;
            this.lblDrop.Location = new System.Drawing.Point(10, 22);
            this.lblDrop.Name = "lblDrop";
            this.lblDrop.Size = new System.Drawing.Size(450, 40);
            this.lblDrop.TabIndex = 0;
            this.lblDrop.Text = "拖放 EXE / MSI 文件到此处";
            this.lblDrop.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.lblDrop.DragDrop += new System.Windows.Forms.DragEventHandler(this.lblDrop_DragDrop);
            this.lblDrop.DragEnter += new System.Windows.Forms.DragEventHandler(this.lblDrop_DragEnter);
            //
            // btnBrowse
            //
            this.btnBrowse.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnBrowse.Location = new System.Drawing.Point(470, 25);
            this.btnBrowse.Name = "btnBrowse";
            this.btnBrowse.Size = new System.Drawing.Size(80, 35);
            this.btnBrowse.TabIndex = 1;
            this.btnBrowse.Text = "浏览...";
            this.btnBrowse.UseVisualStyleBackColor = true;
            this.btnBrowse.Click += new System.EventHandler(this.btnBrowse_Click);
            //
            // lblFilePath
            //
            this.lblFilePath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.lblFilePath.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.lblFilePath.Location = new System.Drawing.Point(10, 68);
            this.lblFilePath.Name = "lblFilePath";
            this.lblFilePath.Size = new System.Drawing.Size(540, 23);
            this.lblFilePath.TabIndex = 2;
            //
            // grpResult
            //
            this.grpResult.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.grpResult.Controls.Add(this.lblConfidence);
            this.grpResult.Controls.Add(this.lblDetectedBy);
            this.grpResult.Controls.Add(this.lblVersion);
            this.grpResult.Controls.Add(this.lblInstallerType);
            this.grpResult.Controls.Add(this.label4);
            this.grpResult.Controls.Add(this.label3);
            this.grpResult.Controls.Add(this.label2);
            this.grpResult.Controls.Add(this.label1);
            this.grpResult.Location = new System.Drawing.Point(12, 118);
            this.grpResult.Name = "grpResult";
            this.grpResult.Size = new System.Drawing.Size(560, 100);
            this.grpResult.TabIndex = 1;
            this.grpResult.TabStop = false;
            this.grpResult.Text = "检测结果";
            //
            // label1
            //
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(15, 28);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(80, 17);
            this.label1.TabIndex = 0;
            this.label1.Text = "安装器类型:";
            //
            // lblInstallerType
            //
            this.lblInstallerType.AutoSize = true;
            this.lblInstallerType.Font = new System.Drawing.Font("Microsoft YaHei UI", 9.5F, System.Drawing.FontStyle.Bold);
            this.lblInstallerType.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(80)))), ((int)(((byte)(160)))));
            this.lblInstallerType.Location = new System.Drawing.Point(100, 28);
            this.lblInstallerType.Name = "lblInstallerType";
            this.lblInstallerType.Size = new System.Drawing.Size(16, 19);
            this.lblInstallerType.TabIndex = 1;
            this.lblInstallerType.Text = "--";
            //
            // label2
            //
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(15, 52);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(44, 17);
            this.label2.TabIndex = 2;
            this.label2.Text = "版本:";
            //
            // lblVersion
            //
            this.lblVersion.AutoSize = true;
            this.lblVersion.Location = new System.Drawing.Point(100, 52);
            this.lblVersion.Name = "lblVersion";
            this.lblVersion.Size = new System.Drawing.Size(16, 17);
            this.lblVersion.TabIndex = 3;
            this.lblVersion.Text = "--";
            //
            // label3
            //
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(15, 76);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(68, 17);
            this.label3.TabIndex = 4;
            this.label3.Text = "检测方式:";
            //
            // lblDetectedBy
            //
            this.lblDetectedBy.AutoSize = true;
            this.lblDetectedBy.Location = new System.Drawing.Point(100, 76);
            this.lblDetectedBy.Name = "lblDetectedBy";
            this.lblDetectedBy.Size = new System.Drawing.Size(16, 17);
            this.lblDetectedBy.TabIndex = 5;
            this.lblDetectedBy.Text = "--";
            //
            // label4
            //
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(280, 28);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(56, 17);
            this.label4.TabIndex = 6;
            this.label4.Text = "置信度:";
            //
            // lblConfidence
            //
            this.lblConfidence.AutoSize = true;
            this.lblConfidence.Font = new System.Drawing.Font("Microsoft YaHei UI", 11F);
            this.lblConfidence.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(200)))), ((int)(((byte)(120)))), ((int)(((byte)(0)))));
            this.lblConfidence.Location = new System.Drawing.Point(340, 25);
            this.lblConfidence.Name = "lblConfidence";
            this.lblConfidence.Size = new System.Drawing.Size(16, 20);
            this.lblConfidence.TabIndex = 7;
            this.lblConfidence.Text = "--";
            //
            // grpParams
            //
            this.grpParams.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.grpParams.Controls.Add(this.lstParams);
            this.grpParams.Location = new System.Drawing.Point(12, 224);
            this.grpParams.Name = "grpParams";
            this.grpParams.Size = new System.Drawing.Size(560, 180);
            this.grpParams.TabIndex = 2;
            this.grpParams.TabStop = false;
            this.grpParams.Text = "静默安装参数";
            //
            // lstParams
            //
            this.lstParams.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.lstParams.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] { this.colSwitch, this.colDesc });
            this.lstParams.FullRowSelect = true;
            this.lstParams.GridLines = true;
            this.lstParams.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
            this.lstParams.Location = new System.Drawing.Point(10, 22);
            this.lstParams.Name = "lstParams";
            this.lstParams.Size = new System.Drawing.Size(540, 148);
            this.lstParams.TabIndex = 0;
            this.lstParams.UseCompatibleStateImageBehavior = false;
            this.lstParams.View = System.Windows.Forms.View.Details;
            //
            // colSwitch
            //
            this.colSwitch.Text = "参数";
            this.colSwitch.Width = 200;
            //
            // colDesc
            //
            this.colDesc.Text = "说明";
            this.colDesc.Width = 320;
            //
            // grpCommand
            //
            this.grpCommand.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.grpCommand.Controls.Add(this.txtCommand);
            this.grpCommand.Controls.Add(this.btnCopyCmd);
            this.grpCommand.Location = new System.Drawing.Point(12, 410);
            this.grpCommand.Name = "grpCommand";
            this.grpCommand.Size = new System.Drawing.Size(560, 60);
            this.grpCommand.TabIndex = 3;
            this.grpCommand.TabStop = false;
            this.grpCommand.Text = "静默安装命令";
            //
            // txtCommand
            //
            this.txtCommand.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.txtCommand.Font = new System.Drawing.Font("Consolas", 10F);
            this.txtCommand.Location = new System.Drawing.Point(10, 24);
            this.txtCommand.Name = "txtCommand";
            this.txtCommand.ReadOnly = true;
            this.txtCommand.Size = new System.Drawing.Size(450, 23);
            this.txtCommand.TabIndex = 0;
            //
            // btnCopyCmd
            //
            this.btnCopyCmd.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnCopyCmd.Location = new System.Drawing.Point(470, 22);
            this.btnCopyCmd.Name = "btnCopyCmd";
            this.btnCopyCmd.Size = new System.Drawing.Size(80, 27);
            this.btnCopyCmd.TabIndex = 1;
            this.btnCopyCmd.Text = "复制";
            this.btnCopyCmd.UseVisualStyleBackColor = true;
            this.btnCopyCmd.Click += new System.EventHandler(this.btnCopyCmd_Click);
            //
            // grpBatch
            //
            this.grpBatch.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) | System.Windows.Forms.AnchorStyles.Right)));
            this.grpBatch.Controls.Add(this.chkComments);
            this.grpBatch.Controls.Add(this.chkUac);
            this.grpBatch.Controls.Add(this.chkErrorHandling);
            this.grpBatch.Controls.Add(this.chkLog);
            this.grpBatch.Controls.Add(this.btnGenerateBat);
            this.grpBatch.Location = new System.Drawing.Point(12, 476);
            this.grpBatch.Name = "grpBatch";
            this.grpBatch.Size = new System.Drawing.Size(560, 80);
            this.grpBatch.TabIndex = 4;
            this.grpBatch.TabStop = false;
            this.grpBatch.Text = "批处理生成选项";
            //
            // chkLog
            //
            this.chkLog.AutoSize = true;
            this.chkLog.Checked = true;
            this.chkLog.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkLog.Location = new System.Drawing.Point(15, 28);
            this.chkLog.Name = "chkLog";
            this.chkLog.Size = new System.Drawing.Size(87, 21);
            this.chkLog.TabIndex = 0;
            this.chkLog.Text = "包含日志";
            this.chkLog.UseVisualStyleBackColor = true;
            //
            // chkErrorHandling
            //
            this.chkErrorHandling.AutoSize = true;
            this.chkErrorHandling.Checked = true;
            this.chkErrorHandling.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkErrorHandling.Location = new System.Drawing.Point(110, 28);
            this.chkErrorHandling.Name = "chkErrorHandling";
            this.chkErrorHandling.Size = new System.Drawing.Size(87, 21);
            this.chkErrorHandling.TabIndex = 1;
            this.chkErrorHandling.Text = "错误处理";
            this.chkErrorHandling.UseVisualStyleBackColor = true;
            //
            // chkUac
            //
            this.chkUac.AutoSize = true;
            this.chkUac.Checked = true;
            this.chkUac.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkUac.Location = new System.Drawing.Point(205, 28);
            this.chkUac.Name = "chkUac";
            this.chkUac.Size = new System.Drawing.Size(99, 21);
            this.chkUac.TabIndex = 2;
            this.chkUac.Text = "管理员权限";
            this.chkUac.UseVisualStyleBackColor = true;
            //
            // chkComments
            //
            this.chkComments.AutoSize = true;
            this.chkComments.Checked = true;
            this.chkComments.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkComments.Location = new System.Drawing.Point(312, 28);
            this.chkComments.Name = "chkComments";
            this.chkComments.Size = new System.Drawing.Size(63, 21);
            this.chkComments.TabIndex = 3;
            this.chkComments.Text = "注释";
            this.chkComments.UseVisualStyleBackColor = true;
            //
            // btnGenerateBat
            //
            this.btnGenerateBat.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnGenerateBat.Enabled = false;
            this.btnGenerateBat.Font = new System.Drawing.Font("Microsoft YaHei UI", 9.5F, System.Drawing.FontStyle.Bold);
            this.btnGenerateBat.Location = new System.Drawing.Point(410, 50);
            this.btnGenerateBat.Name = "btnGenerateBat";
            this.btnGenerateBat.Size = new System.Drawing.Size(140, 25);
            this.btnGenerateBat.TabIndex = 4;
            this.btnGenerateBat.Text = "生成批处理文件";
            this.btnGenerateBat.UseVisualStyleBackColor = true;
            this.btnGenerateBat.Click += new System.EventHandler(this.btnGenerateBat_Click);
            //
            // statusStrip
            //
            this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] { this.lblStatus, this.lblDieStatus });
            this.statusStrip.Location = new System.Drawing.Point(0, 563);
            this.statusStrip.Name = "statusStrip";
            this.statusStrip.Size = new System.Drawing.Size(584, 22);
            this.statusStrip.TabIndex = 5;
            //
            // lblStatus
            //
            this.lblStatus.Name = "lblStatus";
            this.lblStatus.Size = new System.Drawing.Size(32, 17);
            this.lblStatus.Text = "就绪";
            //
            // lblDieStatus
            //
            this.lblDieStatus.Name = "lblDieStatus";
            this.lblDieStatus.Size = new System.Drawing.Size(100, 17);
            this.lblDieStatus.Text = "";
            //
            // MainForm
            //
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
            this.ClientSize = new System.Drawing.Size(584, 585);
            this.Controls.Add(this.statusStrip);
            this.Controls.Add(this.grpBatch);
            this.Controls.Add(this.grpCommand);
            this.Controls.Add(this.grpParams);
            this.Controls.Add(this.grpResult);
            this.Controls.Add(this.grpDrop);
            this.Font = new System.Drawing.Font("Microsoft YaHei UI", 9F);
            this.MinimumSize = new System.Drawing.Size(600, 620);
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "大内静探 - 静默参数探测器 v1.0";
            this.grpDrop.ResumeLayout(false);
            this.grpResult.ResumeLayout(false);
            this.grpResult.PerformLayout();
            this.grpParams.ResumeLayout(false);
            this.grpCommand.ResumeLayout(false);
            this.grpCommand.PerformLayout();
            this.grpBatch.ResumeLayout(false);
            this.grpBatch.PerformLayout();
            this.statusStrip.ResumeLayout(false);
            this.statusStrip.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();
        }

        private System.Windows.Forms.GroupBox grpDrop;
        private System.Windows.Forms.Label lblDrop;
        private System.Windows.Forms.Button btnBrowse;
        private System.Windows.Forms.Label lblFilePath;
        private System.Windows.Forms.GroupBox grpResult;
        private System.Windows.Forms.Label lblConfidence;
        private System.Windows.Forms.Label lblDetectedBy;
        private System.Windows.Forms.Label lblVersion;
        private System.Windows.Forms.Label lblInstallerType;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.GroupBox grpParams;
        private System.Windows.Forms.ListView lstParams;
        private System.Windows.Forms.ColumnHeader colSwitch;
        private System.Windows.Forms.ColumnHeader colDesc;
        private System.Windows.Forms.GroupBox grpCommand;
        private System.Windows.Forms.TextBox txtCommand;
        private System.Windows.Forms.Button btnCopyCmd;
        private System.Windows.Forms.GroupBox grpBatch;
        private System.Windows.Forms.CheckBox chkComments;
        private System.Windows.Forms.CheckBox chkUac;
        private System.Windows.Forms.CheckBox chkErrorHandling;
        private System.Windows.Forms.CheckBox chkLog;
        private System.Windows.Forms.Button btnGenerateBat;
        private System.Windows.Forms.StatusStrip statusStrip;
        private System.Windows.Forms.ToolStripStatusLabel lblStatus;
        private System.Windows.Forms.ToolStripStatusLabel lblDieStatus;
    }
}
