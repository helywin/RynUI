# 0362721 评审修复证据

使用当前隔离 worktree 的 `windows-msvc-headless`，MSVC、Ninja Multi-Config，最多 4 jobs。遵循最新授权，仅创建本地提交，不 push。以下为平台通用 headless 证据，不代表原生 GPU、Linux 或人工输入验收。

## P2-1 / P2-2：滚轮与 Select popup

- 旧代码复现：新增负 Y 下滚断言失败（`scroll clamp or retained identity`）；仅修正符号后继续复现 `Table consumed Select popup wheel`。
- Table 的垂直偏移改为减去 wheel delta。实际 `InputComponentHost::dispatch(ScrollInputEvent)` 验证负 Y 滚到下边界、正 Y 返回顶部，保留行身份且不重新 shape 文本。
- Table host 在 popup hosts 之前注册，使 forward positioning 先完成 Table transform，reverse wheel dispatch 先给 popup。Table cell 保留的 Select 验证 popup 内滚轮只移动菜单、popup 外滚轮移动正文、同一 synchronization 内 popup 跟随 anchor，向上滚动恢复位置；数据更新不重挂载 Select，销毁无交互残留。
- 重新构建 Table 和 portable Select 测试可执行文件后，Debug 2/2（0.89s）、Release 2/2（0.51s）通过：`rynui.table_component`、`rynui.portable.select_component`。忽略的构建日志：`out/066-review-wheel-{debug,release}.log`。
