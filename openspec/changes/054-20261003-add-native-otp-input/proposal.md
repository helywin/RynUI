# Proposal

## Why

Input 单行家族和 TextArea 已补齐共同逻辑与 Windows 原生证据，已有 Input 家族仍缺 OTP。用户要求继续补齐原生桌面功能，本 change 实现完整的分格输入合同，之后继续 visual bidi、Typography 与 Theme 收尾。

## What Changes

- 新增 typed OTPProps/OTP/OTPRef、indexed typed separator，复用 Input 的编辑器、焦点、IME 和 logical retained scene。
- 以 Unicode grapheme 为格子单位，实现 default/controlled 值、动态 length、整段粘贴分发、formatter、mask、部分输入与完成回调。
- 实现选中格子、首次空格焦点、自动前进、左右/空格 Backspace、RTL、disabled/readOnly、原生输入提示与安全卸载。
- 提供三个尺寸、四变体/status/Theme、居中文字与 padding；扩展 Gallery、共同回归与独立 Windows/Linux 证据。
- HTML autoComplete/type、DOM/CSS/event/ref API 不移植；visual bidi 与其他组件后续独立收尾。

## Capabilities

### New Capabilities

- `native-otp-input`：原生分格 Unicode 输入、格式化事务、焦点/IME 与 retained 呈现。

### Modified Capabilities

无。`openspec list --specs --json` 当前返回空主规格目录；不修改未归档 change 的历史合同。

## Impact

新增公开 `include/ryn/otp.hpp` 与内部 OTP model/host，扩展 Input 内部单格配置及 successful user edit hook，连接 WindowComponentServices；不增加第三方依赖、不改 renderer ABI。源码、真实窗口、Linux 证据分别验收，规划完成不代表实现完成。
