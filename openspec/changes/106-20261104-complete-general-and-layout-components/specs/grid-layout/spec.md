## Purpose

为原生窗口提供与 Ant Design 对齐的响应式二十四列布局，使业务界面能通过类型化跨度、偏移、排序和间距表达不同窗口尺寸下的内容关系，保留子组件状态与输入顺序。

## ADDED Requirements

### Requirement: 响应式栅格

Row/Col SHALL 支持 24 列 span、offset、order、push/pull、typed flex、双轴 gutter、wrap、align、justify、RTL 与 xs 到 xxxl 响应式覆盖。断点 SHALL 基于窗口可用宽度；span=0 SHALL 隐藏该分支。

#### Scenario: 调整窗口与重排
- **WHEN** 窗口跨越断点或列配置响应式更新
- **THEN** 在同帧重新测量并放置列，身份与编辑状态保持，隐藏列不能命中或聚焦。
