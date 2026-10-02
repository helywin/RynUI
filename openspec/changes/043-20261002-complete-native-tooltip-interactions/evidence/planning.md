# 规划依据

2026-10-02；本文件记录 source reviewed 和设计，不作为实现证据。

已检查 `include/ryn/tooltip.hpp`、TooltipComponentHost、PointerRouter、WindowComponentServices、Text/Icon mount 与私有 CFF 生成器，及现有 Tooltip tests。主 specs inventory 为空，043 新增扩展 capability，不覆盖 040 lifecycle 合同。当前问题：只有 String title 与 hover/focus、无窗口空白 pointer 观察、32 条 quad 箭头 GPU 呈现灰色。

在线核对锁定 Ant Design 6.6.5 的 [Tooltip API/source](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/index.tsx)、[说明](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/index.zh-CN.md) 和 [placements](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/placements.ts)：title 为内容、触发动作可配置，arrow pointAtCenter 改变角锚点，角箭头使用固定位置。RynUI 保留自己的 typed native API、reactive 属性与窗口资源机制。

用户已经授权“写完 change 就开始改代码”，且明确仅补原生桌面功能。规划完成后直接进入 apply，不等待额外批准。Windows 是当前实际机器，Linux 专属验收独立待办。
