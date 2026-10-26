# Proposal

## Why

RynUI 尚无原生层级数据选择组件。固定 key 与拓扑的首版 Tree 使目录、组织层级等常用流程可以通过真实键盘、指针和滚动操作完成。

## What Changes

- 新增 typed TreeProps、TreeItem、富标题与图标 slots；标题、显隐、禁用和受控状态保持 reactive。
- 实现展开、单选、可选父子级联勾选与半选、disabled 隔离、单 Tab roving 导航和有限 viewport 滚动显露。
- 追加完整 Tree Theme 合同和原生 logical scene host，保留富内容身份及输入边界。
- 首版不提供多选、strict checking、动态拓扑、异步加载、拖放、虚拟化、搜索、RTL、动画、连接线、DirectoryTree 或 Web API；不暴露占位接口。

## Capabilities

### New Capabilities

- `native-tree`: 固定拓扑的原生层级展示、选择、勾选、导航、主题与资源生命周期。

### Modified Capabilities

无。

## Impact

新增公开头文件、组件与模型、Theme 解析及订阅、窗口 host 注册、定向测试和 docs/tree.md。沿用 Core、logical scene、输入与字体路径，不改变 renderer/GPU ABI；不修改 Gallery、README 或全局进度。主要风险是富 slot 用户回调重入销毁、隐藏节点的 popup/session 清理及有限分区裁剪；以实际输入与 scene 测试验证。本次仅在隔离 worktree 做 MSVC HEADLESS Debug/Release 验证和本地英文提交。
