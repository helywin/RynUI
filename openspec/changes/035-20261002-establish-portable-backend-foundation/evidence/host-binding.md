# 宿主与 GPU binding 验收

实际平台：Windows/MSVC，`windows-msvc-debug`，2026-10-02。

默认构建通过，9/9 相关 CTest 通过：SDL text input、text input atomicity、clipboard/commands、platform event pump、platform lifecycle、frame renderer 与架构合同。日志：`out/035-host-build.log`、`out/035-host-tests.log`。

生命周期 fixture 实际调用新的 SdlGpuBinding 和生产 QuadGpuBuffer，验证：宿主只创建 init/window；GPU create 与 claim 各自失败后没有销毁窗口，仍可 poll/读取 metrics；binding 自己清理设备；两次显式构造 binding 使用不同 epoch；GPU resource 先释放，再 release window/destroy device。FrameRenderer 通过 binding 获取设备，非 owner thread 仍拒绝 GPU 调用。

原生 minimal Debug smoke 的 GPU/窗口证据记录在 `out/035-binding-smoke.log`。更全面 Gallery/DPI/字体/输入验收属于本 change 独立 Windows 任务，不由 fake 生命周期或 Recording 替代。
