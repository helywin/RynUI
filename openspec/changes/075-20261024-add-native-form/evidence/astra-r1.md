# Astra R1 修复证据

## Form 字体可表示边界

基线 `1276bbf` 上先加入回归，再执行以下命令（独立官方 MSVC headless 构建树、最多 4 jobs）：

```powershell
& .\out\form-build.ps1 -Targets rynui_form_theme_tests,rynui_form_component_tests -Filter '^rynui.form_(theme|component)$'
```

Debug 结果为 0/2；实际失败消息分别是 `Form resolver accepted a font that rounds to zero logical pixels` 与 `invalid Form font partially published Theme or mutated actual Text/input state`。

修复在 `apply_form_override` 结束处检查最终派生的 `font_size >= 0.5` 且有限，包含直接 override、继承与 component algorithm 的统一出口。Text 先对 logical font size 做 `lround`，再交给 DPI resolver，因此不能用放大后的物理像素掩盖零 logical font。正常指标保持原值。

同一命令 Debug 2/2；加 `-Configuration Release` 后 Release 2/2。新增测试覆盖初始失败清理、响应更新拒绝时 Theme generation/snapshot 与实际 Text typography/scene revisions/geometry/input session 不变、极小正数/阈值相邻值/NaN/infinity、component seed 派生、恢复、真实 glyph 与 1x/2x DPI。此处是 headless font/input 合同，不代表 native GPU 或系统 IME 验收。

`out/form-build.ps1` 仅为本地命令包装：初始化 VS 2026 MSVC 环境，然后运行 `cmake --build --preset windows-msvc-headless-{debug,release} --parallel 4 --target ...` 和 `ctest --preset windows-msvc-headless-{debug,release} -R ... --output-on-failure`；输出全部属于本 worktree。
