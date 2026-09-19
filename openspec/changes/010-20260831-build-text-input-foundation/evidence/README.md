# Input evidence

字段采用单行 `key=value`。`platform-generic-input.md` 只记录可在任一受支持平台执行一次的 API、Unicode、editing、headless interaction、Token、scene、allocation 与 idle 合同；不得把其中的执行平台写成原生窗口验收。

`windows-input.md` 与 `linux-input.md` 分别预留 Win32/D3D12/DXIL 和原生 Wayland/Vulkan/SPIR-V 的系统 IME、候选窗、clipboard、字体、scale 与人工确认结果。`status=pending` 不代表通过；planning-only、跨平台身份、缺少完整 commit SHA 或 `idle_restored=true` 的 passed 记录会被合同拒绝。
