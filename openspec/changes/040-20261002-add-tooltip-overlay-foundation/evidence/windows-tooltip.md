# Windows Tooltip 原生验收

2026-10-02，实际 Windows 11 / MSVC，Ninja Multi-Config，SDL3 / D3D12 / DXIL，系统 scale=1.25。

代码基线为 2d6e90b（Tooltip/API/Theme/Gallery）及本目录对应验收入口；041 尚在独立实施。本记录不将其后 Slider 代码纳入 Tooltip 二进制验收。

## 构建与回归

- windows-msvc-debug：完整 245 项执行，发现 Theme 新字段导致 golden 过期、新增 Gallery anchors 导致三项数量合同过期；更新后重跑四项全部通过，其中 Gallery 两项 26.88 s。其他 241 项已通过，未放宽原有 allocation/input/renderer 合同。
- windows-msvc-release：完整 CTest 245/245 通过，79.64 s。
- 平台通用 headless Debug/Release 38/38 见 [通用记录](tooltip-common.md)，不重复要求 Linux 验证相同逻辑。

## 实际窗口矩阵

[validate_native.py](windows/validate_native.py) 使用 bundled Python/Pillow，把 GPU BMP readback 转 PNG，记录可执行文件前后 SHA256、完整参数、退出码与 PNG 哈希到 [runs.json](windows/runs.json)。Debug/Release 各使用 system 和 1.0/1.25/1.5/2.0 render scale：共 10 次真实窗口运行、120 PNG，全部 exit_code=0，哈希核对通过。

每次运行都通过 SDL normalized mouse/key events 和真实 system font chain 验证：hover 延迟、靠顶边翻转、长 CJK/English、后置 sibling 浮层顺序、Escape、disabled child hover、受控 open/close 两次请求、键盘 focus、不丢 child Enter 激活、Default/Dark/Compact、arrow 开关、resize=760x600 与失活无提示/deadline。诊断均为 gpu_driver=direct3d12、shader_format=DXIL、normalized_events=11、requests=2、child_clicks=1、submits=24、content_runs=1。

已检查 system hover-edge 与 scale-2 Dark 的真实截图，文字、边缘移位和焦点环可见，浮层覆盖后方按钮。箭头当前以共同 logical quad strips 近似，未增加 triangle GPU ABI；这属于基础提示的绘制方案，不声明完整上游视觉逐像素等价。

Linux native Vulkan/Wayland/Fontconfig 无本机证据，tasks 5.1 保持未勾选；自动注入与 GPU readback 不代替系统输入法或用户主观视觉验收。
