# Input evidence

字段采用单行 `key=value`。`platform-generic-input.md` 只记录可在任一受支持平台执行一次的 API、Unicode、editing、headless interaction、Token、scene、allocation 与 idle 合同；不得把其中的执行平台写成原生窗口验收。

`windows-input.md` 与 `linux-input.md` 分别预留 Win32/D3D12/DXIL 和原生 Wayland/Vulkan/SPIR-V 的系统 IME、候选窗、clipboard、字体、scale 与人工确认结果。`status=pending` 不代表通过；planning-only、跨平台身份、缺少完整 commit SHA 或 `idle_restored=true` 的 passed 记录会被合同拒绝。

`windows-input-build.md` 独立记录 9.1 的 fresh configure、Debug/Release build 与 CTest 结果，只能证明 Windows 自动构建和平台分支合同；它必须保持 `manual_ime_visual_result=pending`，不得提前关闭真实窗口 runner 或人工 IME/视觉验收。

`windows-input-automated.md` 独立记录 9.2 四档真实窗口 runner 的 stdout 路径、SHA256 与进程退出码；内部事件自动覆盖不等于系统 IME 或人工视觉通过。
