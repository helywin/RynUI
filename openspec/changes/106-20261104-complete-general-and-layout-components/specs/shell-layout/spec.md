## Purpose

为原生应用建立可嵌套的页面框架，统一页头、页尾、正文与侧栏的测量、剩余空间分配和主题背景，支持窗口尺寸变化与侧栏展开收起且不丢失业务组件状态。

## ADDED Requirements

### Requirement: 页面框架与响应侧栏

Layout SHALL 根据直属 Sider 自动选择排列方向，支持嵌套 Header、Footer、Content；Sider SHALL 支持宽度、受控/默认 collapsed、collapsedWidth、light/dark、breakpoint、反向箭头、自定义/隐藏触发器及变化回调。

#### Scenario: 零宽度与受控侧栏
- **WHEN** 侧栏跨断点折叠到零宽度，或受控触发器请求展开
- **THEN** 可访问触发器仍可操作，受控值由调用方决定，内容和输入生命周期保持安全。
