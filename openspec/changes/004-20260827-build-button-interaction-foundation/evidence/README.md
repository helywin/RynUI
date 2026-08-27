# Button 验收证据格式

Linux 与 Windows 验收分别写入 `linux-button-evidence.md` 和 `windows-button-evidence.md`，不得复制另一平台的运行结果充当本平台证据。字段采用单行 `key=value`，自动合同要求两份文件路径、`platform` 身份和截图路径互相独立。

`status=pending` 仅表示已预留证据清单，不代表通过。完成对应平台的构建、CTest、真实窗口交互和人工视觉核对后，才可填写真实值并改为 `status=passed`。
