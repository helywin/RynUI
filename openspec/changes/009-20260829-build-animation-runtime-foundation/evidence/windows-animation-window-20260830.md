# Windows Animation Runtime 自动化窗口证据

status=automated-passed-manual-pending
change=009-20260829-build-animation-runtime-foundation
git_sha=7c8a6810a31007e23689fff27a84ca12381971ab
os=Microsoft Windows 11 专业工作站版 10.0.26200 x64
toolchain=MSVC 19.51.36256.0 x64
generator=Ninja Multi-Config
dependency_mode=BUNDLED
gpu=NVIDIA GeForce RTX 5080
driver_version=32.0.16.1047
nominal_refresh_hz=240
gpu_driver=direct3d12
shader_format=DXIL
host_display_scale=1.5
font_source=system
font_families=Segoe UI Variable Text,Microsoft YaHei UI
screenshot_policy=not-captured-per-user-request

## Clean build 与 CTest

- `cmake --fresh --preset windows-msvc`：exit 0；识别 MSVC x64、Windows 10.0.26200、BUNDLED SDL3 3.4.14、DXC 1.8.2502 与 Ninja Multi-Config。
- `cmake --build --preset windows-msvc-debug --clean-first --parallel 2`：exit 0；clean 移除 613 个旧产物后完成 622 个 build steps。
- `ctest --preset windows-msvc-debug --output-on-failure`：149/149 passed；前 132 项连续输出通过，尾部 17 项以 `-I 133,149` 独立复验 17/17 passed。
- `cmake --build --preset windows-msvc-release --parallel 2`：exit 0；完成 234 个 Release build steps。
- `ctest --preset windows-msvc-release -R <Windows/animation affected contracts>`：62/62 passed。

## 真实窗口自动 journey

命令统一使用 `rynui_token_gallery.exe --animation-acceptance --acceptance-scale=<scale>`。journey 在真实 Win32/D3D12 窗口中依次切换 Theme motion off/on、injected reduced/normal、Dark、Compact、Brand、loading 与 Default，并等待最后一个 animation deadline 后恢复 idle。

| Config | Scale | Exit | Theme updates | Motion updates | State updates | Scene rebuilds | Submits | Animation frames | Idle waits | Idle after animation |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Debug | 1.0 | 0 | 6 | 2 | 2 | 1 | 315 | 98 | 83 | 2 |
| Debug | 1.25 | 0 | 6 | 2 | 2 | 1 | 256 | 91 | 83 | 2 |
| Debug | 1.5 | 0 | 6 | 2 | 2 | 1 | 189 | 78 | 82 | 2 |
| Debug | 2.0 | 0 | 6 | 2 | 2 | 5 | 220 | 82 | 80 | 2 |
| Release | 1.5 | 0 | 6 | 2 | 2 | 1 | 349 | 103 | 83 | 2 |

四档运行均报告 `gpu_driver=direct3d12`、`shader_format=DXIL`、`animation_acceptance=true`、系统字体链和 `exit_code=0`。2.0 的窄 logical viewport 在 Theme/effect 可见性变化时记录 5 次 scene rebuild，其他档位为 1；当前不把该差异描述为 spinner topology 通过或失败，留给人工视觉与后续诊断。

## DXIL identity

- `glyph.fragment.dxil=d1d889698b6b7e35e6ee1b89d12c5f3e4b3b4ebf5e9364fb3ba4d7f2e6085dfd`
- `glyph.vertex.dxil=1a8a79cb19ad5eb5e4f83fb282ca579bf678f320f7492bad4cb5911a070aff95`
- `quad.fragment.dxil=43801d6550f01bc1960f78ee4b0dd577049f6bbd057e7ab67534a4e6f7e9879e`
- `quad.vertex.dxil=701b28d99314f27af52bd82cdced782d36d87a099af3144540426ffd8d428f31`
- `rounded_effect.fragment.dxil=e215b3f8ae63ddad59fd0e21c574e0306407ab9886858df3ff4b5cde78932ada`
- `rounded_effect.vertex.dxil=91f8dc0303fabf173040590ecfb30be99e89219ca9b8f73b58f63bc70c567236`

## 尚未完成

人工 hover/press/release/leave、keyboard focus、过渡连续性、focus ring 与 hover border 区分、spinner 静态/动态可识别性、四档 DPI 裁切/抖动以及等待动画结束后的窗口 idle 观察仍待用户确认；因此 Windows evidence 尚未设为 `status=passed`，OpenSpec 8.2–8.4 保持未勾选。
