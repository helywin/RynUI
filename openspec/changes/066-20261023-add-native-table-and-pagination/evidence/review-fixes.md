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

## R2：Modal → Table → popup host 顺序

- 基于 `8187e35` 新增集成回归，旧代码实际失败于 `Modal consumed scrollable Table body wheel`（`out/066-r2-modal-before.log`）。Modal 与 Table 均有 overflow，正文滚轮事件经 `InputComponentHost::dispatch` 进入真实 reverse participant dispatch。
- 仅将 Table 的 host 注册行移到 Modal 之后、Tooltip/Select 之前，保留全部 hosts。forward positioning 先完成 Modal 的布局/正文 translation，再应用 Table 的行滚动，最后定位子弹层；reverse wheel dispatch 则先 popup、再 Table、最后 Modal。父任务 main 的 Modal 在 Popconfirm/Tooltip/Select 之前这一相对合同保持不变。
- open Modal 内 `scrollY(80)` 的多行 Table 验证：首帧 body clip 为 80；负 Y 增加 Table scroll 且 Modal scroll 不变；行实际 bounds/translation 在同帧及后续 sync 仍移动并保持 body clip/hit 裁剪；正 Y 恢复。保留 child Select 的 anchor 随 Table 与外层 Modal 滚动跟踪，重叠 popup 内滚轮只移动 Select。正文下部滚轮仍可滚动 Modal，销毁完整清理 Table/Select。既有普通 root Table/Select 回归同时执行。
- 独立 MSVC headless Debug/Release、最多 4 jobs，重新构建并运行 5 项：`rynui.table_component`、`rynui.portable.modal_component`、`rynui.portable.modal_boundary`、`rynui.portable.select_component`、`rynui.portable.tooltip_component`。Debug **5/5**（2.64s），Release **5/5**（1.29s），日志 `out/066-r2-{debug,release}{,-build}.log`。未重复完整 119 项或上一轮 19 项套件。
- format check：508 owned files、0 failures；doctor healthy；全量 strict validate：62 passed、0 failed；diff check 通过。仅本地提交，无 push/PR/子代理，无父工作区/Gallery 修改；验收仍仅限 Windows MSVC headless。

## 主分支集成

父任务以 main6968904 合入6cf4376，保留 Drawer/Popconfirm/TagBadge/Alert/Spin/Progress 与既有 Theme 引用提交路径。注册顺序为 Modal → Table → Popconfirm → Tooltip → Select，既保留所有 hosts，也让 Table 变换先于其行内浮层定位，逆序输入优先交给子层。

五份 golden 由新编译的 Theme algorithm 生成，逐个 JSON 对比合并前 main：仅新增 pagination/table 与整体 identity，全部旧字段相等。Table/Pagination API/theme/component、feedback component、Theme algorithm/runtime、Modal/Drawer/Popconfirm component 的合并后定向回归 Debug10/10（9.68s）、Release10/10（1.62s）通过，日志 `out/table-merge-debug-tests.log` 和 `out/table-merge-release.log`。

clang-format542个源文件零失败，doctor healthy，strict validate67/67，diff/staged check 通过。README中英文和常用组件清单增加 API 文档入口；Gallery 与原生平台验收尚未在本次集成补充，只推送 main。
