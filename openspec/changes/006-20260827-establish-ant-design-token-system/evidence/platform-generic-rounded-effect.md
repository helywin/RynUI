# RoundedEffect 平台通用验收

- 日期：2026-08-27
- OS：Microsoft Windows 11 专业工作站版 10.0.26200（Build 26200）
- Compiler：MSVC 19.51.36256.0，x64
- Configure preset：`windows-msvc`
- Build/Test preset：`windows-msvc-debug`
- Generator：Ninja Multi-Config

## 验收结果

```text
scripts/build-windows.ps1 -Configuration Debug
116/116 passed
exit code: 0

openspec validate --all --strict --no-interactive
6/6 passed
exit code: 0

openspec doctor --json
healthy: true
exit code: 0

git diff --check
exit code: 0
```

新增 `rynui.rounded_effect_math`、`rynui.rounded_effect_store`、`rynui.rounded_effect_scene` 与 `rynui.rounded_effect_allocation_benchmark`。测试覆盖 outer/inset/negative spread、非法值、rounded-rect SDF 解析点与内嵌 golden mask、Gaussian soft edge、透明 outline gap、corner symmetry、3-sigma/AA bounds、translation、DPI scale、部分/完全 ancestor clip、无隐式 content clip、ShadowList 展开、outer/fill/inset 和跨 surface 顺序、destroy/reuse、稀疏 material dirty range、geometry compact、visibility/window/clip cull、容量复用与空 ShadowList。

1,024 个 retained effect 的 Debug benchmark 连续执行 10,000 次 idle compact 与等值 material update，确认零分配、零 dirty upload，并保持 store 容量不增长。普通 `QuadInstance` 继续保持 48 bytes，effect geometry/material 使用独立 store，不给无 effect 控件增加实例带宽。

本文件只记录平台通用 CPU reference、Scene 与 retained store 合同；不作为 Windows D3D12/DXIL、Linux Vulkan/SPIR-V、shader artifact 或真实窗口渲染证据。GPU effect pipeline 属于第 6 阶段。
