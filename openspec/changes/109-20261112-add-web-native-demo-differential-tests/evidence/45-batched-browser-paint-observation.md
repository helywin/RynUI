# 固定浏览器绘制观测的分批读取

- 单次快照按真实 backend node 去除重复查询，分批读取 DOM 与 UA 节点。绘制层按 Chromium 的原顺序出具；截图前后各独立读取一次并核对摘要。
- Ubuntu 24.04 与 Windows 的锁定 Chromium 141.0.7390.37 均通过 `test_full_select_deferred_children_have_real_portal_instances_and_input_receipts`。正常用例包含 10 个检查点，实际 placeholder、额外 popup 包装层和未消费的 renderer 负例均被检出。
- Linux 单独复跑此前超时的 placeholder 负例，worker 用时 108.8 秒，在 120 秒上限内完成。完整测试在 Linux 用时 735.9 秒，Windows 用时 255.7 秒，包含准备与多轮真实故障执行。这是 Web 观测与输入结果，配对视觉和原生 GPU 校准另行验收。
