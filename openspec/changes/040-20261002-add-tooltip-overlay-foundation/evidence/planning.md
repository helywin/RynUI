# 规划证据

2026-10-02，Windows，main，起点 bcb208e。用户授权先写 change 后立即 apply；随后明确要求 Tooltip 与 Slider marks/dots 两项，并补齐已有组件原生桌面功能，Web 专用 API 不移植。

- OpenSpec 1.14.0，全局 CLI；主 spec inventory 空。
- doctor healthy；strict 全量 40/40；git diff --check 通过。
- 已读 ComponentHost traversal、共同服务与 TextHost 布局/同步、Input/Focus/Pointer 合同、Theme/Slider/Gallery；规划涉及的插入点有实际源码依据。
- 原生组件收尾范围见 docs/component-completion.md；本 change 先落地浮层/Tooltip，随后推进 Slider 和其余功能缺口。
- 未将规划或源码审查当作功能实现、平台验收；本机 Windows，Linux 独立项保持 pending。
