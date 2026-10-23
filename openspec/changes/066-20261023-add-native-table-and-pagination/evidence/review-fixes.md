# 0362721 评审修复证据

使用当前隔离 worktree 的 `windows-msvc-headless`，MSVC、Ninja Multi-Config，最多 4 jobs。遵循最新授权，仅创建本地提交，不 push。以下为平台通用 headless 证据，不代表原生 GPU、Linux 或人工输入验收。

## P2-1 / P2-2：滚轮与 Select popup

- 旧代码复现：新增负 Y 下滚断言失败（`scroll clamp or retained identity`）；仅修正符号后继续复现 `Table consumed Select popup wheel`。
- Table 的垂直偏移改为减去 wheel delta。实际 `InputComponentHost::dispatch(ScrollInputEvent)` 验证负 Y 滚到下边界、正 Y 返回顶部，保留行身份且不重新 shape 文本。
- Table host 在 popup hosts 之前注册，使 forward positioning 先完成 Table transform，reverse wheel dispatch 先给 popup。Table cell 保留的 Select 验证 popup 内滚轮只移动菜单、popup 外滚轮移动正文、同一 synchronization 内 popup 跟随 anchor，向上滚动恢复位置；数据更新不重挂载 Select，销毁无交互残留。
- 重新构建 Table 和 portable Select 测试可执行文件后，Debug 2/2（0.89s）、Release 2/2（0.51s）通过：`rynui.table_component`、`rynui.portable.select_component`。忽略的构建日志：`out/066-review-wheel-{debug,release}.log`。

## P2-3：保留行交互顺序

- 新增真实 Tab down/up 回归在旧实现失败：`A/B to B/A did not update actual Tab order`。
- 每次 Table geometry refresh 收集保留行内的交互块，按可见行顺序及其余 inactive 行顺序调用既有 `InteractionRegistry::reorder_after`，锚定在表头交互之后。新增行即使较外部 following control 晚挂载，也回到 Table 的逻辑位置；不重建控件。
- 实际按键验证外部 before → A → B → after、重排后 before → B → A → after、插入后 before → B → C → A → after，以及删除与 Table 销毁后的外部顺序。断言行 Component、Input Interaction、当前焦点与编辑 session 保留，只有新增 C 执行新 cell slot。
- `rynui.table_component` Debug 1/1（0.69s）、Release 1/1（0.34s）通过；日志 `out/066-review-focus-{debug,release}.log`。

## P2-4 / P2-5：隐藏布局和单元格高度

- 两个新增回归在旧代码分别失败：`hidden Pagination introduced extra Flex gap/height` 和 `short Table cell did not stretch to full row height`。五项评审问题均已复现，无跳过项。
- Pagination 根据 hidden 更新 root `out_of_flow`，可见性变化显式失效 parent Measure/Layout。隐藏时清零保留 root geometry 并短路内部 measure/place，外部 LayoutStyle 保留，显示时正常恢复。默认高度与显式 60dp 高度分别验证初始隐藏、显示→隐藏→显示、total/hideOnSinglePage 更新；Text/Pagination/Text 隐藏时只有一个 12dp gap，恢复时 Component/按钮身份和点击保持。
- Table 以 stretch-height 放置 cell，使每个 cell 的实际 bounds 等于整行高度。回归包含同一行 Checkbox、单行 Text、三行 Text，断言全部 cell 的 y/height 与 row 一致，并检查实际 Checkbox/Text bounds 的中心与 row 中心一致。
- 重新构建两项组件测试后，Debug 2/2（1.31s）、Release 2/2（0.37s）通过；日志 `out/066-review-layout-{debug,release}.log`。

## 最终 focused 验证

所有涉及的测试可执行文件均已按最终代码重新构建（`cmake --build --preset windows-msvc-headless-debug/release --parallel 4 --target …`）。随后执行：

```text
ctest --preset windows-msvc-headless-debug --parallel 4 -R '(table_|pagination|portable\.(select_component|tooltip_component|modal_component|modal_boundary|tabs_component|focus_order|focus_state|focus_lifecycle|interaction_registry|flex_features|layout_engine|theme_algorithm|prop|reactive_source_lifecycle|backend_boundaries)$)'
ctest --preset windows-msvc-headless-release --parallel 4 -R '(table_|pagination|portable\.(select_component|tooltip_component|modal_component|modal_boundary|tabs_component|focus_order|focus_state|focus_lifecycle|interaction_registry|flex_features|layout_engine|theme_algorithm|prop|reactive_source_lifecycle|backend_boundaries)$)'
```

- Debug **19/19**（3.11s），Release **19/19**（2.51s）。包含 Table/Pagination 新回归、Table token/public API、Ant Design 6.6.5 全部 golden 主题兼容、相关 popup hosts、focus/registry、Flex/layout、reactive 生命周期及 portable backend boundary。没有重跑原先已通过的完整 119 项套件。
- `python scripts/format-code.py --check --clang-format <clang-format-22>`：508 owned files，clang-format 22.1.3，0 failures。
- `openspec doctor --json`：healthy；`openspec validate --all --strict --no-interactive`：62 passed，0 failed；`git diff --check` 通过。
- 最终日志位于忽略的 `out/066-review-final-{debug,release}{,-build}.log`；每阶段 format/doctor/spec 日志亦位于该 worktree 的 out。源码未改 Theme、CMake、CPU scene v3、GPU ABI v2、全局 Gallery 或父工作区。
