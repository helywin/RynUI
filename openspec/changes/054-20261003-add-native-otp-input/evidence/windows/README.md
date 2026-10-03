# Windows OTP 原生验收

2026-10-03，实际 Windows/MSVC、`windows-msvc` Ninja Multi-Config。共同逻辑在 `windows-msvc-headless` Debug/Release 已通过完整 92/92，见 [Gallery 回归](../common/gallery.md)。

Debug/Release 构建 Gallery 与 24 个相关 native 测试目标，CTest 各 24/24（50.66 / 16.11 秒）。`native-ctest.log` 包含两配置的 build、CTest 与默认 Gallery `--smoke`：实际 D3D12/DXIL，系统缩放 1.25，153 stable IDs、172 live samples、63 个 Theme，退出 0。

两配置各系统缩放与显式 1、1.25、1.5、2，共十轮专用 `--otp-acceptance` 真实窗口。每轮八组 OTP、36 个 editor、四变体/三个尺寸/status，顶层 Content 只执行一次；18 张 GPU readback、36 次提交，共 180 张 PNG。`runs.json` 保存参数、退出码、当时 EXE 与规范 UTF-8/LF log 的 SHA256，PNG 为原始 bytes hash。初始像素 1600×1100，resize 后 1420×1000。

每轮覆盖受控 partial/complete 回调及同值 echo、实际系统 clipboard 的 grapheme 粘贴/截断、formatter 大写/custom mask/敏感复制阻止、preedit 不分格且拥有导航、IME native area 与 stale stamp、提交后前进、indexed separator/RTL、动态 length 删除活跃 preedit owner并恢复隐藏后缀、前缀身份不变。六个 pointer 事件经实际 SDL 队列与平台转换，resize 后命中目标格并全选；popup/关闭、三轮 idle 无 deadline及销毁后的 ref/editor/scene/interaction/animation 清理均通过。组合文本由规范化事件驱动并使用真实 SDL session/area；没有将其描述为人工 OS 候选窗口操作。

审核 `debug-system/dark.png`、试运行 `out/054-otp-trial/preedit.bmp`、`release-scale-2/window-pointer-hit.png` 和 `release-scale-1.25/popup.png`：mask、居中、preedit、四变体、RTL/separator、缩放/resize 和浮层位置正确。缺失 emoji glyph 按共享 fallback 显示 replacement，原 Unicode 及 grapheme 边界保留。

复核（依赖 Pillow）：

```powershell
& 'C:\Users\jiang\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' openspec/changes/054-20261003-add-native-otp-input/evidence/windows/validate_native.py --verify-only
```

最终输出 `All saved EXE, native log, dimensions and GPU readback identities match`。不带 `--verify-only` 可重跑十轮；后续重建 Gallery 会改变 EXE hash，历史复核应按本次提交重建或重新生成。Linux 独立待实际机器验收，Input visual bidi 在后续 change 收尾。
