# 组件 Theme 迁移与 Button Focus 平台通用验收

- 日期：2026-08-28
- OS：Microsoft Windows 11 专业工作站版 10.0.26200（Build 26200）
- Compiler：MSVC 19.51.36256.0，x64
- Configure preset：`windows-msvc`
- Build/Test preset：`windows-msvc-debug`
- Generator：Ninja Multi-Config
- Ant Design：6.5.0 / `740ad964dc2397f33e40944367b0536a7314cc32`

## 验收结果

```text
scripts/build-windows.ps1 -Configuration Debug
120/120 passed
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

Text、Button、Flex 与 Space 已从挂载组件所属的 resolved `ThemeScope` 读取 typed Token。测试覆盖 Text 颜色的 Material-only 更新、font family/weight/size/line-height 的字体链重解析与 reshape/measure、Button color/effect/layout/typography 的分组订阅，以及 Flex/Space preset gap 对自定义 gap 和 sibling 的隔离；所有 Theme 更新均保持 content slot、Component 与 Scene identity。

Button 覆盖 Default、Primary、Danger 的 normal/hover/active、disabled 与 loading precedence，并将 shadow 与 focus 同时保留为独立 `RoundedEffect`。CPU 与 GPU reference 验证 focus-visible 为 1 logical px 透明 gap 加 3 logical px hollow ring，150% simulated display scale 下分别转换为 1.5 与 4.5 physical px；pointer focus 不显示 focus-visible，keyboard focus 显示。

Button 与 Layout 公开示例已同步 effect GPU buffer，并将 effect resources 传入统一 Scene renderer；Button headless frame contract 验证 effect partial upload、draw 与 idle。`rynui.component_theme_contract` 禁止稳定组件新增 `DefaultThemeSnapshot`、`default_theme_snapshot`、直接 `resolve_theme()` 和旧 solid focus quad 引用。最小 Default Theme adapter 仅保留给迁移兼容测试。

本文件只记录组件 Theme 迁移、headless Scene/GPU contract 与 simulated DPI 的平台通用结果，不作为 Windows D3D12/DXIL 或 Linux Vulkan/SPIR-V 的真实窗口、GPU/driver 和截图证据。
