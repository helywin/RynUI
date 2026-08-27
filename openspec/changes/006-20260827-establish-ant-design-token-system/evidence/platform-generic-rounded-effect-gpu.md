# RoundedEffect GPU pipeline 平台通用验收

- 日期：2026-08-27
- OS：Microsoft Windows 11 专业工作站版 10.0.26200（Build 26200）
- Compiler：MSVC 19.51.36256.0，x64
- Configure preset：`windows-msvc`
- Build/Test preset：`windows-msvc-debug`
- Generator：Ninja Multi-Config
- Shader source SHA256（LF normalized）：`b511e7298ee39d55d58c63e67b1bdf01e47557c01e72eb7e89929d0b53be5a6b`

## 验收结果

```text
scripts/build-windows.ps1 -Configuration Debug
119/119 passed
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

`rounded_effect.hlsl` 作为 DXIL 与 SPIR-V 的唯一源文件生成并部署 vertex/fragment artifacts；lock、reflection 和 freshness contract 锁定同一 source hash、entry point 与 112-byte instance layout。CPU/HLSL reference 覆盖 outer、inset、hollow outline、straight-color alpha coverage、透明与半透明背景、重叠 layer 和 alpha edge。

GPU contract 覆盖独立 effect buffer、power-of-two growth、partial/full upload、失败重试、cleanup、Scene batching、clip/cull、zero-effect 与 idle frame；无 effect frame 不提交 effect draw。logical-to-device conversion 在 100%、150% 与 200% simulated display scale 下覆盖 geometry、blur、spread、offset、outline 与 1 physical px antialias guard。

本文件只记录 shader-independent reference、artifact/reflection、fake backend、Scene/GPU contract 与平台通用测试结果，不作为 Windows D3D12/DXIL 或 Linux Vulkan/SPIR-V 的真实窗口、GPU/driver 和截图证据。两类平台专属证据仍由第 9、10 节分别验收。
