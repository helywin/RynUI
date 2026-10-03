# Windows Input 原生验收

2026-10-03，在 Windows/MSVC、windows-msvc Ninja Multi-Config 上完成本 change 的 Windows 验收。平台通用逻辑采用 windows-msvc-headless，最终完整 CTest Debug 85/85（119.52 s）、Release 85/85（23.29 s），见 common/visibility-regressions.md；没有用这些结果代替原生窗口检查。

受影响原生 CTest Debug 20/20（38.04 s）、Release 20/20（14.89 s），包含系统字体、Input/Password/Search/Tooltip、主题、交互和 retained scene 回归。native-ctest-debug.log 与 native-ctest-release.log 保存构建及测试输出。默认 Gallery --smoke 两个配置均退出 0，实际 D3D12/DXIL、系统缩放 1.25、system+bundled 字体链；133 stable IDs、152 live samples、61 Theme content runs、27 编辑器。gallery-debug-smoke.log/gallery-release-smoke.log 保存完整记录。

## 专用真实窗口

两个配置分别使用系统缩放、显式 1、1.25、1.5、2，合计十次 --input-features-acceptance 运行。每次保存 23 张实际 GPU readback、46 次提交、16 个注入 SDL 队列后经正常平台路由处理的 pointer 事件。runs.json 记录当时 EXE SHA256、参数、退出码、日志 SHA256、全部 230 张 PNG 的 SHA256/像素尺寸。日志 hash 使用 UTF-8/LF 规范化内容以适应 Git checkout 换行规则，EXE/PNG 使用原始 bytes。初始窗口读回为 1600×1100，resize 后为 1420×1000。

每轮覆盖四变体、Dark/Compact/status、grapheme/soft max/formatter/history、showCount 隐藏及恢复、原生输入用途与输入区域、IME stamp 切换、clearDisabled 恢复、自定义图标、Hover/受控 Password、Search Input/Clear 来源、resize 后真实指针命中、Tooltip popup/关闭、无 deadline 的三次 idle 和销毁。断言 content_runs=1、输入 editor/scene/引用身份保留，销毁后无残留 editor/scene/interaction。日志中 native_starts/native_areas 来自实际 SDL text-input 端口；组合文本通过标准化编辑事件驱动，未将其描述为人工 OS 候选窗口验收。

审核 debug-system/initial.png、release-scale-2/count-hidden.png、debug-scale-1.5/password-visible.png、release-scale-1.25/popup.png；脚本同时检查关键状态图片 hash 不同。窗口发现的隐藏字形、重新启用 HitTest 和系统字体 U+FFFD 缺口已修复，最终证据重新生成。

## 复核

在相同构建产物仍存在时：

```powershell
& 'C:\Users\jiang\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' openspec/changes/052-20261003-complete-native-input-features/evidence/windows/validate_native.py --verify-only
```

最终输出：All saved EXE, native log, dimensions and GPU readback identities match。依赖 Pillow；系统 Python 无该模块时使用 Codex bundled Python。脚本不重跑 CTest。以后修改并重建 Gallery 会改变 EXE hash，需按对应历史提交重建或重新执行脚本生成新证据，不应把当前 EXE 当作历史验收产物。

Linux 项保持未勾选，要求在真实 Linux 机器独立完成其窗口/GPU/system font/input 验收。
