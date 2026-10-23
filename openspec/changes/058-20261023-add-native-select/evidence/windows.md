# Windows 原生 Select 验收

日期：2026-10-23。实际 Windows、MSVC x64/Ninja Multi-Config、`windows-msvc` Debug/Release。实际窗口使用 `direct3d12` / `DXIL`，系统 display scale=1.5；UI sans 为 Segoe UI Variable Text，来源与 face index 在每次日志记录。

20次运行全部 exit=0：Debug/Release × Light/Dark × system/1/1.25/1.5/2。每次18张真实 GPU readback，合计360张 PNG；每张文件与日志哈希记录在 `windows/windows-debug.json`、`windows/windows-release.json`。日志使用 UTF-8 LF 归一化哈希。两个 EXE 的 SHA256 与当前本地构建匹配。

每次实际运行包含：

- 16个 retained Select，字符串/数值、controlled value/open、clear、disabled/loading、空列表、四变体、三尺寸、warning/error 与 Component Token。
- pointer 选项提交、清空与回写，中文搜索、composition Enter 不选择、首次 Escape 取消组合而保持弹层；keyboard 跳过 disabled 并提交数值。
- 60项菜单 wheel、End 自动滚入最后一项，翻转至窗口底部选择器上方；窗口 resize 后重新约束菜单，窗口失活清理焦点/输入会话。
- 根 content runs=1，选择器/候选 identity 和组件总数不变；3次 idle polls 不继续请求或提交帧；dispose 后节点、文字、interaction、ref、输入与动画资源清空。

视觉抽查了 Light 2倍中文搜索/底部翻转与 Release Dark 2倍状态页，文字、候选、边框和窗口范围正常。全部 PNG 另经解码、非空像素范围与文件哈希检查。这里的输入是向实际窗口关联的组件服务注入逻辑事件，**不是人工系统IME或完整平台事件来源验收**。

复现：

```powershell
python openspec/changes/058-20261023-add-native-select/evidence/run_native_matrix.py Debug
python openspec/changes/058-20261023-add-native-select/evidence/run_native_matrix.py Release
python openspec/changes/058-20261023-add-native-select/evidence/verify_native_evidence.py --check-executables
```

Debug/Release native build 均通过；两配置受影响 Select/Input/SDL/platform CTest 各11/11通过（4.28s / 1.99s）。common 与 Gallery 已在前阶段独立验收，无需重新运行相同逻辑合同。clang-format 22.1.3 检查469个自有源文件通过；doctor healthy、strict 58/58、diff check通过。

Linux native、Vulkan/SPIR-V 与该平台系统输入/字体尚无实际证据；5.2保持未勾选，不影响已经完成的 common/Windows 项，后续常用组件工作继续。
