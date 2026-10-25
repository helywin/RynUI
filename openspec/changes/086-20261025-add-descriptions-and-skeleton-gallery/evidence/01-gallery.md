# 086 Gallery 实施证据

## 范围与构建

- 起点 `bfbb49234c945c52988b8474838d3ac88bbc84ec`；仅在隔离 worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-921d9c2d`、分支 `t3/descriptions-skeleton-gallery` 实施。本地规划提交 `3ccc86fd`；按用户连续授权进入 apply，无 Git 远端操作。
- 本树锁定 Ant Design 源码复制自授权的 `D:/code/RynUI/out/upstream-ant-design-6.6.5`，实际 HEAD 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`；读取 Descriptions basic/size 与 Skeleton complex/children/element 源码。
- 仅复制原生 `_deps/*-src`、validation fonts、Unicode 17 数据至本树；未复制 CMakeCache/Ninja tree、build/subbuild、exe/obj，未写 ROOT/out。prepared source 参数全部指向本树 `_deps`。
- 本树重新 configure：`windows-msvc`，MSVC 19.51.36256.0，Ninja Multi-Config；build 使用 `windows-msvc-debug`、`--parallel 4`，target 为 `rynui_token_gallery_frame_tests`、`rynui_ant_design_reference_catalog_tests`。

## 行为与检查

追加两个样例函数：Descriptions 项目资料编辑/保存，Skeleton 正文编辑/分享/发布和静态占位。所有恢复控制位于隐藏内容之外；公开 InputRef 实际由外部编辑按钮调用。共同生产 anchor 直接定位两个真实标题。

`--details-loading-only` 使用真实目录 Pointer 请求/生产 anchor/jump、Button 和 Switch 事件、Select 键盘与选项点击、TextCommitted/CompositionChanged 及旧 session 拒绝。按标题所属 owner 和完整 ComponentId descendants 选择真实控件。验证 label/content glyph 分区、实际 ordered quad 或共同 clipped fill、hit clip、Button/Switch 外扩焦点四边 coverage，以及 component/editor/scene/interaction/slot/ref 保留。

新 focused 最终矩阵完整退出 0：Default（smoke step 4）、Dark（0）、Compact（1）×1200×900 / 560×900，共六组通过，原始日志见 `focused.txt`。Compact 实际走到共同 clipped fill，夹具按实际 ordered effect 和 coverage 识别两次；没有放宽精确形状数量或改动 Core。

旧代表仅运行一次：`--forms-only`、`--panels-only`、`--time-picker-only` 均退出 0。保留 Form 原 `1.`、`1.0000004`、`1.234567891234` 精度检查、Collapse body-owned Select 选择和 Date/Time 原无 blur 同值 reset 断言；仅扩展旧目录 destination 断言以识别两个新生产 anchor。TimePicker 日常输入、popup wheel、authority 和完整/不完整 draft/preedit 的无 blur reset 实际通过。

```text
cmake --preset windows-msvc <本树 prepared source 参数>
cmake --build --preset windows-msvc-debug --target rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests --parallel 4
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --details-loading-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --panels-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --time-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
```

inventory 在 1200×60000 下仅用于精确盘点，可达性由上面的真实有限视口检查承担。新 inventory 断言与完整 main 的旧数量断言同步，不放宽布局或行为条件：

```text
stable_ids=358 navigation=93 buttons=367 interactions=1412 visible_interactions=671 live_samples=291 theme_content=67 inputs=151 selects=24 modal_drawer=13 selection=47 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=3 form_items=6 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1 descriptions=1 skeletons=4
```

八 contracts 最终 8/8（0.58 秒），包含 generator check/self-test；独立 generator `--check` 通过。`compare-baseline.py` 核实 334 旧 IDs 顺序、31 旧 `add_*` 函数体及调用顺序、71 非目标 overlay 条目，以及 Core/API/Theme/goldens/CMake、父 README/完成记录均保持基线。

本树日志在 `out/086-*.log`。中间编译修正了新样例的 LayoutStyle margin 写法；新夹具首次长运行主动停止以移除多余同步，之后 Compact 数量断言揭示夹具需要统计共同 clipped fill，未修改 Core。一次重链接遭运行中 EXE 占用（LNK1168），待旧回归结束后串行 build 已成功。

## 本地收口

实现提交 `f74ee57ca9b0974e6d36a2e5506d94376ba8acbd` 后工作树干净。clang-format 22.1.3 全仓检查 674 个自有源文件、0 failures；OpenSpec 1.14.0 doctor healthy，strict validation 84/84，generator consistency、基线比较和 `git diff --check` 通过。最后仅提交本收口记录与完成后的任务状态。

```text
python openspec/changes/086-20261025-add-descriptions-and-skeleton-gallery/evidence/compare-baseline.py
python tools/update_ant_design_gallery_catalog.py --check
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

实际 Python 为 `C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`，OpenSpec 为 `C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd`。未运行完整 Gallery main；只在源中接入新 focused 流程一次，完整运行交父统一验收。

## 验收边界

只证明本机 MSVC Debug 的原生组件运行时、逻辑场景与合成输入事件；没有运行 GPU、原生窗口、Linux、系统 IME、AT 或完整 frame/native smoke。未重复未改 Core 的历史全套；完整约 1001 秒 frame/native smoke 与全新 Astra 独立审查交父安排。不创建 PR，不 archive，不访问 Git 远端或 push。
