# Tasks

## 1. 平台通用：共用 API、焦点与系统提示

- [x] 1.1 提取兼容的 typed InputPropsBase，实现 InputRef、autoFocus、focus/blur 回调与 reactive 原生用途/大小写/autocorrect；补 docs/input.md 及引用/重入/分支/组合输入/旧 stamp 合同测试，在 Windows windows-msvc-headless Debug/Release 构建并运行 input_public_api、input_component、input_session、password、search、focus、typography_interaction 测试，format-code/doctor/strict validate/diff 通过后提交。

## 2. 平台通用：四变体与 Theme

- [x] 2.1 实现四变体及对应 Token/override/dirty dependency、focus-visible、Underlined 底边和 Compact 接缝，Password/Search 透传；补 Token/retained geometry/不重建/尺寸/Theme/status 合同测试与文档，在上述 common preset Debug/Release 运行 input/theme/space_compact/search 相关 CTest，格式及规格校验通过后提交。

## 3. 平台通用：统计与单次编辑裁剪

- [x] 3.1 实现 showCount、scalar/grapheme/custom 统计、formatter、软 max、exceedFormatter 及退休/重入安全的单次编辑事务，补清空/统计/自定义 suffix 明确布局；测试 Unicode、IME、粘贴、硬/软限制、controlled、undo/redo、异常/卸载/重入和无关组件不执行，补文档，在 common Debug/Release 运行 text_editor/input/history/clipboard/display/typography 相关 CTest，格式及规格校验通过后提交。

## 4. 平台通用：清空、Password、Search 操作

- [ ] 4.1 实现 clearDisabled/clearIcon/onClear、Password reactive 显隐开关/停靠/hover/custom IconSource/prefix/suffix、Search custom searchIcon/Clear 来源/全部共用属性与连接外观；补动作禁用/捕获/焦点/受控回写/卸载/组合输入/Compact 合同测试和文档，在 common Debug/Release 运行 affected input/password/search/icon/button/space_compact/typography tests，格式及规格校验通过后提交。

## 5. 平台通用：Gallery 集成

- [ ] 5.1 添加稳定 ID 的四变体、统计、焦点和动作样本，更新 README/组件收尾表并明确 TextArea/OTP/bidi 尚待下一 change；在 Windows common Debug/Release 跑完整 headless CTest 与 Gallery frame 合同，format-code/doctor/strict validate/diff 通过并记录 evidence 后提交。

## 6. 原生分平台验收

### Windows

- [ ] 6.1 使用 windows-msvc Debug/Release 构建受影响 native tests 和 Gallery，运行原生 affected CTest、default smoke 及专用真实 D3D12/DXIL 窗口验收；记录四变体/Theme/status/count/自定义图标/清空/密码/Search/焦点与系统属性、1/1.25/1.5/2 和系统缩放、resize/popup/idle/dispose 的 GPU 读回 hash、截图及日志，审核图像并运行可复核脚本后独立提交 Windows evidence。

### Linux

- [ ] 6.2 在实际 Linux 机器使用 linux-native Debug/Release 完成受影响 native CTest、Gallery smoke、对应窗口/GPU/DPI/system font/input 验收与读回证据，独立提交 Linux evidence；不重复已通过的 common 逻辑，也不由 Windows 结果代替。
