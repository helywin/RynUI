# 平台通用：Input 状态材料与固定阴影槽位

2026-09-12；任务 7.1 的 Token 图层消费，以及 7.2 的静态状态子集。动画尚未接入，不把静态状态测试当作 7.2 完成证据。

## 实现

- default/hover/focus/disabled/read-only/error/warning 从 internal InputTokenSet 解析；placeholder 使用独立 0.25 alpha Token，不再乘 editable foreground 的 alpha。
- error/warning border、hover border、active shadow 和 caret 使用对应 status Token；disabled 保留 status border，但取消 active shadow、使用 disabled background/foreground。
- read-only 可保留 selection focus 与 active style，不启动编辑 session。pointer 和 keyboard focus 都只使用 outlined Input 的 activeShadow；额外 outline 保持隐藏，与锁定 `variants.ts` 的 `outline: 0` 一致。
- selection background/foreground、placeholder、caret 直接消费 typed Token。Input 不再订阅无关的全量 map/alias 或 Text 尺寸/颜色；只保留自身 typed Token 与 font family/weight 依赖。
- 使用 hover pointer 计数，最后一个 pointer 离开后才清除 hover。
- 每个 Input 固定 19 个 rounded effect identity：8 个 outer shadow、border、background、8 个 inset shadow、预留 outline。每种 shadow list 按 CSS 首层在上顺序绘制；outer 在 fill 之前，inset 在 fill 之后且使用内边界。空列表、outer/inset 切换都不增删 layer。
- 缓存容器 bounds/clip/geometry/material/shadow 状态，selection-only 更新跳过未变化的 19 个效果同步。
- Theme 发布前拒绝通过 mutable ShadowLayer 字段构造的非法 blur/kind/offset/spread，保留原 snapshot 与通知计数。

## 验证

Windows/MSVC `windows-msvc-debug` 构建成功。Input source/component/GPU 三项通过（2.25 秒），覆盖 Default/Dark × 三种 status、pointer/keyboard focus、read-only、disabled、多 pointer hover、八层 outer/inset 混合列表及空列表切换；状态变化不增加 shaping、measure 或 composer rebuild。

之前同一图层实现的短程 256 Input 基准：普通 selection 和 composition-selection 各 100 次，均为 0 C++ heap allocation，分别 264/274 毫秒。最终包含生成 metadata、精确订阅和非法 shadow 拒绝的全量 CTest 196/196 通过（226.82 秒）；其中 256 Input 的普通 selection 和 composition-selection 各 20,000 次零分配基准通过（112.96 秒），容量、所有节点 shaping/measure/placement、composer、无关 HitTest 与 GPU range 断言均满足。任务 7.1 因此完成；7.2 的动画仍保持未完成。

生成器只将已消费的 16 个 Input padding/font/hover/active/shadow Token 标记为 runtime；`addonBg` 仍未实现，不把 prefix/suffix 混称 addon。原生窗口、IME、真实 GPU 视觉和动画不属于这些证据。
