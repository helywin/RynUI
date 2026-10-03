# Windows 原生验收

2026-10-03，实际 Microsoft Windows 11 专业工作站版 10.0.26300 / x64，MSVC，Ninja Multi-Config，BUNDLED；preset `windows-msvc`，Debug/Release。共同逻辑结果见 `../common/gallery.md`，不在本阶段重复运行完整 headless。

## 构建与平台集成

`native-ctest.log` 保存 configure、affected targets 的双配置构建、CTest 和默认 Gallery `--smoke`。`default_font_chain`、`sdl_text_input`、`text_clipboard`、`sdl_event_adapter`、`platform_event_pump`、`text_input_atomicity` 各配置 6/6，Debug 0.77s、Release 0.60s。默认 Gallery 使用实际 Win32 窗口与 D3D12/DXIL，系统显示缩放 1.25，159 stable IDs / 178 live samples。

专用窗口第一次试跑发现默认系统链缺少 Arabic/Hebrew coverage，已在平台字体发现中补充静态 Segoe UI/Tahoma/Arial 候选，仅在已有 coverage 缺失时加载。此机实际链为 Segoe UI Variable Text / Microsoft YaHei UI / Segoe UI，未借用测试用 Arabic/Hebrew 字体。新增 Windows 原生字体测试同时核验启动、尺寸/粗体/等宽 resolver 缓存及 DPI 刷新后的 Arabic/Hebrew coverage。Linux 对应 Fontconfig `ar`/`he` 查询代码已加入，尚无 Linux 原生运行证据。

专用 fixture 的预期提交文本曾把 Hebrew Bet 误写为 Arabic Beh，已拆成明确的 Arabic 与 Hebrew 字符串片段；实现输出的逻辑替换正确。最终双配置重建保存在 `native-rebuild.log`。

## 十组真实窗口 / GPU readback

运行 `validate_native.py`：Debug/Release × system / 1 / 1.25 / 1.5 / 2，共 10 组全部退出 0。每组 19 个场景、38 次 GPU frame submissions，初始 readback 为 1600×1100，resize 后六个场景为 1420×1000。共 190 个 PNG 来自 `SdlSceneRenderer::save_frame_bmp` 的 GPU readback，再无损转换；不是 fixture 合成或浏览器截图。

`runs.json` 保存命令、EXE SHA256、UTF-8/LF 规范化日志 SHA256、每张 PNG 原始字节 SHA256 与尺寸。脚本检查十组完整库存、实际 render scale、正数 native start/area 次数、不同状态的图像区别及全部 hash。最终 `--verify-only` 核验当前 EXE、日志与 190 张图像的身份。

场景覆盖 Arabic/Hebrew/数字/括号、Title/Paragraph 与硬/软换行、7 个 editor、双向键盘 caret、Home/End、原生逻辑 clipboard、非连续 selection、IME preedit/commit/undo、Password/OTP mask、TextArea scroll 与 SDL input area、reactive direction、Dark/Compact、resize 后 SDL pointer affinity、Tooltip 浮层、无 deadline/无额外 submit 的三次 idle poll、dispose 与 stale session 拒绝。根 Content 只执行一次，editor/ref/scene/interaction 身份保留。resize 后 pointer 经过三个 SDL fixture 事件与实际 event adapter；键盘与 composition 是规范化输入 fixture，原生 SDL session、input area 和 clipboard 使用实际平台服务。

已目视审核系统缩放下的分段 selection/preedit、Release 2× 的 Dark/Compact 与 TextArea 选区、Release 1× resize 后的 Tooltip：阿拉伯文字形连接、混合文字、数字/括号、选区空隙、mask 和浮层均正常。190 张图像的库存、尺寸、状态区别和 hash 由脚本逐张核验。

此证据不包含物理键盘、OS 候选窗口或主观 IME 使用验收，也不替代 Linux/Wayland 的窗口、系统字体、GPU/input/DPI 验收；task 5.2 保留未勾选。

## 复核命令

```powershell
python openspec/changes/055-20261003-complete-native-bidirectional-text/evidence/windows/validate_native.py
python openspec/changes/055-20261003-complete-native-bidirectional-text/evidence/windows/validate_native.py --verify-only
```

Python 需 Pillow。重新构建会改变 EXE hash；`--verify-only` 只适用于与本次捕获相同的二进制，重建后应重新执行十组捕获。
