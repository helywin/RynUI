# Windows TextArea 原生验收

2026-10-03，在实际 Windows/MSVC、`windows-msvc` Ninja Multi-Config 上完成。本 change 的共同逻辑最终采用 `windows-msvc-headless`：Debug 87/87（157.36 s）、Release 87/87（26.44 s），见 [quad-topology.md](../common/quad-topology.md)。

## 构建与原生回归

`windows-msvc` Debug/Release 构建 Gallery 与 15 个相关测试目标；原生 CTest 连同 Gallery frame 合同各 16/16，Debug 41.18 s、Release 13.18 s。`native-ctest.log` 保存两配置输出。默认 Gallery `--smoke` 两配置均退出 0：实际 D3D12/DXIL、系统缩放 1.25、Segoe UI Variable Text/Microsoft YaHei UI/BundledLatinFallback；141 stable IDs、160 live samples、62 次 Theme content 初次执行。

## 十轮真实窗口

Debug/Release 各系统缩放与显式 1、1.25、1.5、2，共十轮 `--text-area-acceptance`。每轮 20 张 GPU readback、40 次提交、12 个经实际 SDL 队列/平台转换的 pointer 事件和 1 个 SDL wheel 事件；全部 200 张 PNG 与日志、当时 EXE 的 SHA256/参数/退出码保存在 `runs.json`。初始像素尺寸 1600×1100，window resize 后 1420×1000。日志 hash 使用规范 UTF-8/LF 内容，EXE/PNG 使用原始 bytes。

每轮覆盖四变体/size/status、Dark/Compact、跨行 selection/caret reveal、Plain/primary Enter 与 undo、按行 preedit coverage 与 IME ownership、CRLF commit、autoSize maxRows、clear/count/minRows、无 wrap 双向 wheel 不 shape/raster、primary capture resize、组件/window resize 后二维 pointer 命中、Tooltip popup/关闭、三次 idle 无 deadline、销毁后的 ref/editor/scene/interaction/animation 清理。顶层 Content 只执行一次，编辑器和场景身份保留。native_starts/native_areas 来自真实 SDL text-input 端口；组合文本通过规范化事件驱动，没有将其描述为人工 OS IME 候选窗口验收。候选区域为可见 caret 行，向整数原生坐标转换允许边缘向外取整一像素。

审核 `debug-system/selection.png`、`debug-system/preedit.png`、`release-scale-2/resized-area.png`、`release-scale-1.25/popup.png`：逐行 selection、底部 counter、clear、grip、Theme 和浮层位置正确。字体缺失 glyph 按既有 fallback policy 显示 replacement；Unicode 编辑和 grapheme 合同不依赖彩色 emoji 字体。

真实窗口发现的等量 glyph 提交后过期 quad 范围已修复并补共同回归，最终全套证据重新生成。横向 resize 示例父 Flex 使用 Start 对齐；外部显式宽度/父强制 Stretch 仍按布局约束优先。

## 复核

```powershell
& 'C:\Users\jiang\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' openspec/changes/053-20261003-add-native-text-area/evidence/windows/validate_native.py --verify-only
```

最终输出：`All saved EXE, native log, dimensions and GPU readback identities match`。依赖 Pillow；脚本也可不带 `--verify-only` 重跑十轮。后续重建 Gallery 会改变 EXE hash，复核历史产物应按本次提交重建或重新生成证据。Linux 项保持未勾选，等待实际 Linux 机器的 window/GPU/input/system font/DPI 证据。
