# Animation Runtime 验收证据

平台通用合同记录在 `platform-generic-animation-runtime.md`，只需在一个受支持正式 preset 执行一次。Windows 与 Linux 的 clock/display adapter、真实窗口、GPU、DPI、输入和系统字体结果分别写入 `windows-animation-runtime.md` 与 `linux-animation-runtime.md`，两份平台证据不得共用身份或路径。

字段采用单行 `key=value`。`status=pending` 只表示预留清单，不代表通过；`planning-only`、缺少 lifecycle completion 或 last-animation idle 的记录会被合同拒绝。平台通用证据不得冒充真实窗口验收。
