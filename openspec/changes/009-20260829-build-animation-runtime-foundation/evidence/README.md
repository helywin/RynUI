# Animation Runtime 验收证据

平台通用合同记录在 `platform-generic-animation-runtime.md`，只需在一个受支持正式 preset 执行一次。Windows 与 Linux 的 clock/display adapter、真实窗口、GPU、DPI、输入和系统字体结果分别写入 `windows-animation-runtime.md` 与 `linux-animation-runtime.md`，两份平台证据不得共用身份或路径。

字段采用单行 `key=value`。`status=pending` 只表示预留清单，不代表通过；`planning-only`、缺少 lifecycle completion 或 last-animation idle 的记录会被合同拒绝。平台通用证据不得冒充真实窗口验收。

Windows 人工观察可分别启动以下常驻模式；三种 motion 参数互斥，`--acceptance-scale` 支持 `1.0`、`1.25`、`1.5` 与 `2.0`：

```powershell
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe --acceptance-scale=1.5
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe --acceptance-scale=1.5 --motion-disabled
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe --acceptance-scale=1.5 --reduced-motion
```

窗口标题与退出 telemetry 都会标识 effective motion mode。`--animation-acceptance` 仍用于自动 on/off/reduced journey，不能与常驻人工模式组合。
