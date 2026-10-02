# Compact Radio 与 Addon 阶段（平台通用）

实际环境：Windows/MSVC，`windows-msvc-headless`，2026-10-03。

RadioButton 和 RadioGroup Button options 接入最近组合外角、尺寸、focus/wave 与状态 seam；RadioGroup 显式尺寸保持优先，纯组合 RTL 不重复测量。SpaceAddon 提供 typed 内容/size/四 variant/disabled/status；基于当前 Theme 的全局 map/alias/字体/padding/radius/lineWidth 派生视觉，语义背景复用主题调色实现。公开 InputStatus 移到共享头，枚举值保持；InputVariant 本阶段用于 Addon。

参考锁定的 [Addon](https://github.com/ant-design/ant-design/blob/6.6.5/components/space/Addon.tsx) 和 [Addon style](https://github.com/ant-design/ant-design/blob/6.6.5/components/space/style/addon.ts)：Outlined 背景与边框、Filled 状态背景/禁用覆盖、Borderless/Underlined 的透明无边框、三尺寸及连接外角。使用共同 logical rounded effects，未增加组件上传路径或 renderer ABI。

## 验证

- Debug：16/16，通过，6.70 秒。
- Release：16/16，通过，5.06 秒。
- 测试范围：space_compact、radio_features、radio_public_api、selection_component、theme_runtime、space_public_api、space_header_isolation、input_component、password_component、search_component、focus_order、focus_lifecycle、pointer_route、tooltip_component、rounded_effect_math、rounded_effect_allocation。
- mixed Compact 实际覆盖 Small/Middle/Large × H/V × LTR/RTL 的 12 个组合，Addon/Password/Search/RadioButton 的尺寸、首尾角、单一共有边和 block 宽度。
- 中文与 emoji selection/copy/paste 保留会话；富浮层关闭清理 IME/focus/capture；错误/警告 Filled 背景与默认 palette 一致，Dark 更新保留文本 scene/slot，禁用覆盖、纯 RTL 测量复用、idle 无多余 geometry 更新、非法挂载/更新恢复及零约束无非法 effect。
- 零约束发现 Addon token radius 大于实际矩形时可能发布非法 geometry；已按实际矩形 clamp。窗口服务正视口限制保持，零约束使用 LayoutEngine 入口。
- 22.1.3 格式检查、OpenSpec doctor/strict validation 和 diff check 通过。

4.1 平台通用实现完成。Gallery 与真窗口/GPU 验证分别属于 5.1、6.1/6.2，不由本文件替代。
