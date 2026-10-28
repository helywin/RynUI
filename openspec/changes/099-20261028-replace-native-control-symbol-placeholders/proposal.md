# Proposal

## Why

用户要求操作标识使用正式图标，不再以 `>`、`^` 等文本字符模拟箭头。当前 DatePicker、Menu、Pagination 与 Table 仍有这类实现，会使外观和字形覆盖依赖系统文本字体。

## What Changes

- 用现有内置 Ant Design Icon 替换日期导航、菜单展开、分页翻页／跳转和表格排序标识。
- 保留普通日期、页码、标题文本，保留受控状态、操作事件、Theme 和组件身份。
- 补充图标来源、状态切换和保留身份的回归。按用户要求，当前只改源码，不占用桌面、不运行构建／GUI；未执行的验证保持未完成。

## Capabilities

### New Capabilities

- `native-control-icons`: 内置导航和排序操作使用项目图标资源，与文本字体回退分离。

### Modified Capabilities

无；当前没有同步的相关主 spec。

## Impact

影响四个 component host 及定向测试；无公开 API、Theme schema、renderer ABI、新依赖或图标资产生成变化。滚动卡顿仍是单独优先事项，本 change 不把图标替换当作性能修复。
