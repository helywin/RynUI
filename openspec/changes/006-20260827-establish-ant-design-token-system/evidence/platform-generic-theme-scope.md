# Theme Scope 平台通用验收

- 日期：2026-08-27
- OS：Microsoft Windows 11 专业工作站版 10.0.26200（Build 26200）
- Compiler：MSVC 19.51.36256.0，x64
- Configure preset：`windows-msvc`
- Build/Test preset：`windows-msvc-debug`
- Generator：Ninja Multi-Config
- Ant Design：6.5.0 / `740ad964dc2397f33e40944367b0536a7314cc32`
- Catalog SHA256：`b7e1e43a9400c42a7e545a0afa1b0a3117e1a352e2970c27870ac58e9181cd25`

## 验收结果

```text
scripts/build-windows.ps1 -Configuration Debug
112/112 passed
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

新增测试覆盖公开 `ThemeProps` 与 typed content slot、非法 string config 编译失败、Host Default snapshot、透明单层/多层 scope、`inherit=false`、sibling isolation、异常 slot、destroy/reuse、跨线程失败、reactive `Prop<ThemeConfig>`、typed Token identity subscription、逐字段 immutable snapshot diff、nested override masking、stale subscription、错误更新原子回滚和全类 dirty phase 映射。

`rynui.theme_allocation` 在 MSVC Debug 下执行 10,000 次等值 Theme update，确认零分配、零 frame request、零 invalidation；局部 Text color 更新只进入 Material queue，不进入 Text 或 Measure/Layout queue。Theme wrapper 不创建 retained Node，Theme update 不触发 Structure，也不重跑 Host content。

本文件只记录平台通用 Theme scope/runtime 合同；不作为 Windows D3D12/DXIL 或 Linux Vulkan/SPIR-V 真实 GPU 证据。
