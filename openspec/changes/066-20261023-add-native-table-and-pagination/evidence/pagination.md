# Pagination 平台通用证据

运行平台：Windows，MSVC 19.51，Ninja Multi-Config，当前隔离 worktree 的 `out/build/windows-msvc-headless`；build/CTest 最大 4 jobs。

- Debug：`rynui.pagination_component` 与 `rynui.portable.theme_algorithm`，2/2 通过。
- Release：`rynui.pagination_component`，1/1 通过。
- 覆盖受控拒绝/回写、总数收缩、零总数、size_t 极值、禁用、键盘激活、11 槽保留、隐藏、Theme 继承/覆盖/颜色局部失效/非法更新回滚，以及回调销毁和全部资源释放。
- 五组 Theme golden 只加入 Pagination 字段及相应 identity；锁定 source、已有组件 token 和 renderer ABI 不变。
- `format-code.py --check`、OpenSpec doctor、全量 strict validate、`git diff --check` 通过。

未执行原生 GPU、Linux、DPI/人工视觉或 IME 验收；首版不含内置每页数量选择器和跳页输入框。
