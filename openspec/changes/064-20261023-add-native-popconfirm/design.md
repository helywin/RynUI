# Design

## Context

父提交 4e258e2 已有 Tooltip 的纯定位函数、Select 的交互 window layer 和 Modal 的逻辑 owner 输入边界。锁定 Ant Design 6.6.5 commit 为 4a39f54842eade4e565ab336ef6097cd7e723cdd；参考 popconfirm/index.tsx、PurePanel.tsx、popconfirm/style/index.ts 与 popover/style/index.ts。

## Goals / Non-Goals

实现 proposal 的最小可用合同。保留 slots，开关仅切换 branch active；不扩展 renderer、平台 API 或通用 overlay 抽象。Gallery/README/全局完成状态由父任务维护。

## Decisions

- 独立轻量 model 区分 open、受控请求和强制 suppression，避免把请求去重与边界清理混在一起。confirmLoading 只锁确认，确认回调后的状态重新读取。
- host 拥有 trigger wrapper、popup、标题/说明/警告图标和小尺寸 Button；popup 挂在逻辑 owner 下并设 window layer，箭头复用原生字体 glyph 和 Tooltip 定位。未采用 Tooltip host 包装，因为其提示输入和交互焦点合同不同。
- window layer 的有效优先级取自身 Token 与非 Modal 弹层祖先优先级加层数的最大值，使用 int64 避免极值溢出；这使默认1050的子 Select 位于1060的 Popconfirm 之上，仍保留原有 Modal 分组和无关层的排序。
- 使用 Popconfirm 独立 ThemeConfig，默认 elevated 背景、padding12、gap8、description gap4、圆角8、14/22 字体和 warning 图标、z-index1060。继承/reset、独立 seed/algorithm、颜色/尺寸/字体/阴影/顺序五类订阅；Button 自身视觉继续遵循 Button Token。
- 输入命令在子 Select/Tooltip 之后、Modal 之前处理。真实 Button 自身处理键盘激活；非交互 trigger wrapper 提供 focusable 入口。焦点恢复前检查 generation、eligible、branch 与边界；所有用户回调后重新获取 owner。
- 使用 windows-msvc-headless preset、Ninja Multi-Config、MSVC Debug 聚焦 model/API/theme/host/Modal 合同测试，构建并发不超过4。平台通用结果只记录一次；Windows/Linux 原生窗口、GPU/DPI/系统输入另列待验收，不把 fixture 当作人工 IME 结果。

## Risks / Trade-offs

- 回调销毁或重开 → 使用稳定 id 和回调副本，副作用后重新获取 state。
- 富内容过大 → 限制窗口内测量和裁剪；首版不引入弹层滚动容器。
- 受控属性不响应关闭请求 → 普通请求由应用决定；边界/隐藏/失活另行 suppression，直到显式状态更新或触发。
