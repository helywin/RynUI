## Purpose

为原生应用提供持续可访问的浮动操作入口，覆盖独立按钮、展开菜单和返回顶部，统一复用主题、输入与生命周期机制，使交互不依赖 Web 容器或平台渲染分支。

## ADDED Requirements

### Requirement: 浮动操作与分组

组件 SHALL 支持默认/主色、圆/方形、图标与内容、提示、徽标、禁用及点击；支持窗口边缘定位和可选拖动。Group SHALL 支持常驻、点击、悬停展开，四向放置及受控 open，保持键盘可达。

#### Scenario: 分组及生命周期
- **WHEN** 用户展开分组后禁用或销毁组件
- **THEN** 隐藏分支不再接收输入，焦点、捕获、弹层与回调被安全清理。

### Requirement: BackTop 原生滚动目标

BackTop SHALL 使用显式原生滚动目标，按阈值显示，支持滚动进度、时长和 reduced motion；滚动由目标回调执行，不绑定 SDL 或 renderer。

#### Scenario: 返回顶部
- **WHEN** 目标超过可见阈值并激活 BackTop
- **THEN** 目标最终收到零偏移；进度和可见性跟随目标状态，结束后不持续请求帧。
