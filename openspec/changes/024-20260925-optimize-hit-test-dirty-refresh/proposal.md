# Proposal

## Why

Windows D3D12 Gallery 滚动的五进程基线显示整帧 CPU 平均中位数约 16.8 ms，其中布局与场景同步阶段约 14.0 ms；命中刷新仅约 5 µs，并非该窗口的主要耗时。独立规模基准显示，`HitTestSnapshot::refresh` 对每条交互记录遍历整批 dirty Node，并反复向上寻找祖先；大量交互与 dirty Node 同时出现时，工作量会随两者的乘积增长。

## What Changes

- 为命中快照的批量 dirty 刷新增加固定规模、固定 seed 的 CPU 基准，并保存 Gallery 真实窗口滚动的改造前后观测及环境信息。
- 在保持命中顺序、祖先影响、generation 身份和既有输入合同的前提下，让大批 dirty 刷新先建立本轮节点标记，再沿每条记录的祖先链检查，避免逐记录重复扫描整批 dirty Node。
- 保留少量 dirty Node 的低开销路径；覆盖重复、失效、slot 重用、嵌套交互和无关节点的等价性测试。
- 报告局部刷新与 Gallery 整帧的各阶段结果，并明确 GPU 执行时间仍未测。

本 change 只优化批量命中快照刷新。空间查询索引、共享滚动 transform、布局依赖缓存、GPU 上传事务和 staging arena 仍需各自的证据与独立 change。

## Capabilities

### New Capabilities

- `hit-test-dirty-refresh-efficiency`: 批量节点失效后命中快照刷新的身份、结果等价性和工作量合同。

### Modified Capabilities

无。

## Impact

涉及 `src/input/interaction_registry.*`、命中测试与 benchmark、Gallery Windows 滚动证据；不改变公开 C++ API、绘制顺序、视觉输出或依赖锁。主要风险是旧 generation 与新 slot 的混淆、祖先影响遗漏和小批量路径的额外分配；通过定向回归、零分配验收及正式 Windows MSVC Release 测量验证。
