# Design

## Context

动机见 proposal。已检查 Select 的 retained 行与菜单 viewport、Tooltip 的 position_tooltip、Popconfirm 的焦点/受控请求与 Modal 逻辑边界。当前分支 22e40d3 已含 Tooltip suppression 修复；共同 child-layer 排序由 Popconfirm 3ace95a 提供，父任务合并，避免重复修改。

## Goals / Non-Goals

使用 C++20 typed Props / Prop<T> 与 typed trigger/icon slot。普通 label、disabled、selection、open 更新保持行、slot 与 scene identity。首版 items 拓扑在 mount 时固定，结构变化通过现有 If/Reactive scope 显式重挂载。vertical 为平面导航，子菜单仅 Inline（Dropdown 使用 Inline），不伪装为完整 Ant Design Web API。

## Decisions

- Menu 项带稳定 String key；挂载前递归验证非空/重复 key、类型和深度。组/分隔线不可选，disabled 祖先禁用后代。selectedKeys / openKeys 分别固定 controlled/uncontrolled，回调是请求，拒绝时视觉不提交。
- root 仅一个 Tab stop，行参与 pointer 与可编程 focus。Up/Down/Home/End 跳过非操作项；Right 展开并进入子项，Left 收起或回父项；Enter/Space 激活；焦点行按需滚入内部 clip。无需 VDOM 或每帧重建。
- Menu/Dropdown Token 单独配置，默认从现有 Theme alias/map 派生，分颜色/metrics/typography/effects/order 失效。新增接线保持局部；LayoutStyle 只改变外部约束。
- Dropdown trigger 使用 typed slot；Button 的 Enter/Space 交给 FocusManager 完成原始 onClick，wrapper 通过其 activation 观察打开，不能在 command filter 吞掉激活。Down/Up 可直接开菜单。关闭请求、实际隐藏与环境 suppression 独立；初次 hidden Modal 挂载不设置 dismissed，边界激活时重新 refresh。已经展示后失去边界则 suppression，父层恢复不复活。
- popup 是逻辑 trigger 后代的 window layer，按窗口 clip 定位，使用 position_tooltip 翻转/钳制；Menu 自己裁剪/滚动。外部点击不抢回焦点，Escape/命令关闭恢复原 trigger focus；嵌套层用逻辑 ancestry 判定 inside，先让更深的菜单消费 Escape。
- wheel 从最上层 hit 沿 component ancestry 寻找最近 Menu，遇到子 window layer 边界则交回对应 popup host，不依赖 participant/mount 顺序。子 popup 到达滚动边界不向遮挡的父 Menu 泄漏；露出的父菜单区域仍可滚动。popup 自然尺寸测量后，以最终内宽/内高重测 Menu；活动行因响应式禁用重新协调时触发 Layout reveal，保留节点 identity。
- 所有资源由 scope cleanup 释放，调用用户 callback 前复制值并在后续访问时重取 generation；无轮询 deadline。

## Risks / Trade-offs

- 受控拒绝与边界恢复混淆 → 分别测试拒绝、环境关闭、初始隐藏与二次打开。
- nested popup priority → 不复制共同改动，测试 own Dropdown 嵌套与 Modal，记录父集成依赖。
- 文本很长/列表很高 → 行内容 clip 与菜单 viewport scroll；不承诺虚拟化性能。
- 原生平台证据不足 → 本阶段仅 Windows MSVC / Ninja Multi-Config 的 windows-msvc-headless Debug/Release 合同；GPU/Linux/IME 留在父集成范围外，不作通过声明。

## Migration Plan

新增 API 不修改既有公共签名。规划先验证并提交，继续 apply 实现与聚焦验证后本地提交；依最新用户策略由父任务合并、验证和推送 main，子分支不再推送。最后文档/evidence 收口。失败可回退本 change 提交，不改 renderer ABI。
