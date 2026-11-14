# 完整输入消费与七字体链：Windows

2026-11-14，Windows/MSVC，`windows-msvc-headless` Debug/Release；真实 native 使用 `windows-msvc` Release、hidden D3D12/DXIL。Web 使用锁定 Chromium 141.0.7390.37、独立 headless context 和显式七字体链。

- 显式 Python 机制集合 **147/147**。包括首次实际 seed、后续静态 options、动态反馈、实例/slot 归属、字体来源、旧 run/capture/fence 与五个实际 device metrics 字段的正负控制。
- 相关 HEADLESS CTest Debug/Release 各 **9/9**；随后 run/capture nonce 修正的 native protocol/input/observation 三项分别复验 **3/3**，不累加为新的覆盖数量。使用既有 `rynui_demo_differential_protocol_tests`、`rynui_demo_differential_native_input_tests`、`rynui_demo_differential_native_observation_tests`、page acceptance、Select complete 与 selection i18n targets。
- `RealAdapters.native` 分别执行完整 Button/Input/Select pilot：**4/3/10 个检查点**，三个场景均通过 `runner_evidence.native` 的 source/binary/readback/resources 检查、`validate_timeline` 和独立语义 oracle。实际 DPR=1.5，逻辑 1280×900 对应物理 1920×1350；组件/窗口尺寸未从图片推导。
- Web 显式完整 Select 场景 **1/1**，其中错误 placeholder、额外 popup wrapper、绑定但未消费子控件三个真实故障均被检出；旧功能回归 **4/4**（含五个官方 export smoke）；字体浏览器测试 **6/6**。Input 在新 nonce/新进程下重跑，捕获 hash 一致。字体检查包含实际 Latin/CJK 500、400/600、emoji400，以及缺500/缺emoji/regular冒600的拒绝。

浏览器入口为 `python -B -m unittest discover -s tests -p demo_differential_web_adapter_tests.py` 的完整 Select 与上述四项定向过滤，以及 `demo_differential_web_font_tests.py`；准备路径通过 `RYNUI_WEB_PREPARED/RYNUI_WEB_FONTS` 显式传入。C++ 使用对应 Debug/Release build/test presets，仅运行相关 targets/CTest。clang-format 22.1.3、OpenSpec doctor/strict 与 diff 检查通过。

修复保留真实关闭 popup 的实例、草稿及 owner，区分字符串 slot 与嵌套控件，并保留 group 标题的内部不可选 marker 与子选项 disabled。motion-disabled 停止 UA caret 闪烁但保留可见光标；未隐藏 caret、修改其颜色、掩膜截图或覆盖漂移缓存。原始失败、日志、图片和完整身份只留 ignored out。

这些是独立输入、行为与实际捕获证据，**不是校准后的 paired 视觉或生产七轮 meta 通过**。独立多材质/shadow/focus 校准仍未完成；Linux 本轮 GPU、系统 IME/AT、显示器 present 与完整动画变体未验证。相关任务及产品 paint/conic/nonRound 能力保持原状态。
