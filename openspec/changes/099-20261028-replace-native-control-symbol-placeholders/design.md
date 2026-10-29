# Design

## Context

动机见 proposal。冻结父为 `5357c51b81829ae07040da664b04330226f85417`。Tree、Select、Tabs 已使用 Icon；DatePicker 的四个导航 label、Menu 的 arrow_label、Pagination 的翻页／跳转 label 和 Table 的 sort_labels 仍使用普通 Text 符号。项目已有 848 个内置 Ant 图标和独立图标字体，不需要新增资源或依赖。

## Goals / Non-Goals

**Goals:** 替换上述操作标识并补充源图标／代际回归，保留公开 API、Theme 和事件语义。

**Non-Goals:** 不改变文本标点、用户文本、自定义富 slots、专用 Tooltip 矢量箭头、renderer 或资源系统；不以此宣称滚动性能或用户尚未定位的所有问号问题已解决。

## Decisions

1. DatePicker 用 DoubleLeft/Left/Right/DoubleRightOutlined；Menu 用 reactive Right/DownOutlined；Table 用 reactive ColumnHeight/ArrowUp/ArrowDownOutlined。均沿用原语义文字样式和同一 retained 节点。
2. Pagination 前后页直接使用 Left/RightOutlined；中间按钮通过固定 Text 和可见 Icon slot 在页码与 EllipsisOutlined 间切换，不为页码生成矢量、不重建按钮。额外 Icon slot 的数量变化必须在后续 inventory／frame 合同中如实校验。
3. 2026-10-28 用户使用电脑期间只做轻量源码修改；2026-10-29 用户明确恢复实施并允许短暂窗口测量。后续验证使用官方 MSVC、Ninja Multi-Config、windows-msvc-headless Debug/Release，并单独记录 Windows 原生显示结果。
4. 原生图标使用既有 snapshot 模式的 `--snapshot-control=menu|table|pagination|date-picker`：定位真实实例，经过生产命中和指针事件触发操作，截图后退出。Menu/Table/Pagination 验证键盘焦点；DatePicker 保留导航项不聚焦及输入焦点不被夺取的原合同，不强行让被动导航项获得焦点。该诊断只在显式 snapshot 参数下运行。

## Risks / Trade-offs

- [图标与文本宽度不同] → 检查有限尺寸居中、真实 glyph/fill/hit/适用 focus，保持原按钮尺寸和 Theme。
- [分页角色动态变化] → 固定 slots，覆盖跳转→页码→跳转的身份与受控操作回归。
- [用户机器正在使用] → 仅在授权后短暂运行窗口并自动退出，不向其他应用发送鼠标或键盘输入。
