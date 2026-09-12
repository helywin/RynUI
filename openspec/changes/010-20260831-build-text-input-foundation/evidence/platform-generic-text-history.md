# History 与 controlled reconcile 验证

2026-09-12，Windows 11 / MSVC 19.51.36256 x64，正式 `windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config。

## 已实现合同

- 最多 128 个完整 before/after transaction，历史文本 arena 最大 1 MiB；descriptor 与 bytes 均按环形顺序存储。reserve 根据预期单值尺寸预热，不为每个 transaction 单独持有字符串分配。
- 连续 text commit 在同一 merge epoch 合并；selection jump、cut/paste、composition 和显式 submit/batch boundary 结束合并。Undo/Redo 恢复原始 value 与 selection，修改后清除 redo，超额淘汰最旧完整 transaction，单个 transaction 超过预算则拒绝且不改 value。
- composition/candidate update 不写 history；composition commit 只增加一个事务。剪切的 Undo 恢复命令发起时选区，即使平台回调期间选区变化。
- 相同 emitted value/revision 保留 caret、composition、history；显式携带旧 revision 的回显忽略，旧 owner generation 的回显拒绝。只保存最近一次 emitted snapshot，不累积推测值列表。
- 不带 revision 的不同 authoritative value 作为外部新基线处理，取消 composition、clamp selection、清除不可再对应当前基线的 undo/redo；不把它猜成旧回显。相同值保留历史，不触发第二次 mutation。reconcile 层自身没有用户 callback，Input 的 callback 接入属于阶段 5。

## 测试结果

- 正式 Debug build 通过。
- history/reconcile/clipboard/session/editor/依赖合同：12/12 CTest 通过（1.46 秒）。
- 环形存储测试：1,000 次修改后保留 128 个事务，逐个 Undo/Redo 检查 UTF-8 value；按字节预算验证 52 个 20,000-byte 事务、1,040,000 bytes 的保留上限与 wrap readback。
- oversized history 经 editor 拒绝；Undo 准备路径 27 个分配失败点验证 cursor、value、revision 不变。
- history：20,000 cycles，0 allocations，undo_count=128，payload_bytes=2048，arena_bytes=16384。
- controlled echo：10,000 cycles，0 allocations。
- 既有 editor：10,000 cycles / 90,000 operations，0 allocations，owner/capacity 稳定；106 个 mutation preparation 失败点保持原子性。
- OpenSpec doctor、strict validate 和 `git diff --check` 为提交门禁。

以上不代替 Windows 原生窗口或 Linux Wayland 证据；没有更新分平台验收 checkbox。
