# Proposal

## Why

桌面详情、编辑表单需要保留内容并从窗口边缘展开的原生 Drawer。061 已有窗口模态边界、焦点恢复与滚动正文，可共享此运行时而避免第二套栈。

## What Changes

- 新增受控 Drawer、四边 placement、width/height、typed title/content/footer、关闭请求与显示生命周期。
- 固定 header/footer、滚动 body、窗口尺寸夹紧与 retained 内容；与 Modal 共用唯一模态栈、输入边界和焦点恢复。
- 新增独立 Drawer Theme Token 与精确失效、API/model/theme/runtime 聚焦测试和使用文档。
- 首版不含 loading、动画、push displacement、静态 service、拖动、销毁重建、RTL 或平台可访问性桥接。Gallery 与原生平台集成由父任务处理。

## Capabilities

### New Capabilities

- `native-drawer`: 原生受控边缘面板及其 retained 生命周期、主题和模态交互。

### Modified Capabilities

无。

## Impact

新增公开 drawer.hpp；局部扩展现有 Modal host 作为共享模态容器，不改变 Modal API、CPU scene v3/GPU ABI v2 或 renderer。主要风险为混合嵌套层的焦点恢复及过小窗口的裁剪，使用针对性运行时测试验收。构建仅使用当前 worktree 自有目录。
