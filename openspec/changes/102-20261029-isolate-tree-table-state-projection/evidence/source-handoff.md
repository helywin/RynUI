# 102 源码交接：source-only，未构建

日期：2026-10-30（Asia/Shanghai）。这是首批隔离 worker 的未经构建源码交接，不是功能验收或性能结论。`tasks.md` 保持 **0/5**，没有勾选任何任务。

## 精确来源

- 工作区：`C:/Users/jiang/.t3/worktrees/RynUI/t3-f68fa5ee`。
- 起始及结束 HEAD：`673d7181bcc116ebc36d2995cf8c8ced35b3eff2`。
- 审查代码基线：`dd3e71f775fb8d4f1f46aeeff5ea444ba816e54d`。
- 结束实际 branch：`t3/tree-table-projection-tuning`；起始读回为 `t3/f68fa5ee`，本 worker 没有执行分支改名或 Git 写操作。
- 开始及结束运行配置均已读回：`codex / gpt-6-astra / reasoningEffort=ultra / serviceTier=priority`；没有切换模型。
- 已读本树 `AGENTS.md`、apply skill、OpenSpec 1.14.0 的 status / instructions apply / list，以及全部五个 contextFiles。apply 状态为 `ready`、schema 为 `spec-driven`。
- 所有编辑仅位于此工作区。没有 children，没有 commit / push / remote / PR / archive，没有修改父全局文档、README 或 tasks。

## 待导入的确切文件

| 状态 | 路径 |
| --- | --- |
| M | `src/component/tree_component.cpp` |
| M | `src/component/tree_component.hpp` |
| M | `src/component/tree_model.cpp` |
| M | `src/component/table_component.cpp` |
| M | `src/component/table_component.hpp` |
| M | `tests/CMakeLists.txt` |
| 新文件 | `tests/support/state_projection_fixture.hpp` |
| 新文件 | `tests/state_projection_tests.cpp` |
| 新文件 | `tests/state_projection_benchmark.cpp` |
| 新文件 | `openspec/changes/102-20261029-isolate-tree-table-state-projection/evidence/source-handoff.md` |

`tests/CMakeLists.txt` 只有两个 executable 和六个专项 CTest 的注册。父线程可单独合并这 12 行注册，避免与其他 worker 的 CMake 修改冲突。

## 源码改动

### Tree / TUNE-PERF-003

- hover 通过完整 interaction identity 定位索引，只将旧/新行加入呈现队列；focus 保留 roving entry、键盘 reveal 和 `intact` 守卫，退出整树 refresh 路径。
- 缓存 shown 投影供 measure/place/keyboard 使用；整树 refresh 使用 shown bitmap 与 key 集合，TreeModel 的 normalize/shown/checks 不再在逐行循环内线性扫描输入 key 列表。
- `mark_scene_structure_dirty()` 只随真实 branch activation 变化触发。行背景保留固定透明槽位，hover 不增删 effect；focus outline 延续原来的固定槽位。
- 真实模型/Theme 更新仍走完整刷新；实际行尺寸变化进入呈现队列，根 bounds/translation/clip/opacity 或输入隔离变化使整批呈现失效。
- 完整 `intact` / generation、富 slot、禁用节点 conduction、input block、零尺寸及嵌套 wheel 逻辑均保留。未修改原历史测试及断言。

### Table / TUNE-PERF-004

- 数据或接受的新 sort authority 推进 projection revision；排序结果和分页切片分开缓存。selection/bordered/page 复用排序结果；page/pageSize 只刷新切片。
- selected membership 使用持有 key 的集合，visible membership 使用已投影的 row shown 状态。Tab 重排仍按实际 retained descendants 生成，核对完整 ComponentId；可见行后接隐藏行并保持外部邻居顺序。
- 排序复制不可变记录快照、sort 和 comparator；在每次用户比较器调用前后检查完整 owner identity 与 revision。重入请求排队，过时排序不提交；销毁后的旧 state 由局部 shared ownership 保活到退出，但不会写入替代 generation。
- 同 key 新记录继续更新原 row signal；增删只变动相应 retained row。数据绑定、新 cell slot、Theme/控件 signal 交付后补 owner/row 存活检查。
- 没有修改排序图标选择、公开 API、Theme 配方、golden、shader、RC/font 或 Gallery。

## 计数和回归源码

新增 `rynui_state_projection_tests` 的六个 case：

- `tree_local`：128/512/1024 节点，通过实际 hit test、pointer dispatch、keyboard dispatch 检查旧/新行呈现计数、零 projection、零 composer rebuild 和完整节点身份。
- `tree_invalidation`：hover 后 disabled conduction、checked/half、展开、可见性、Theme 字号/字体族更新及实际 Text 布局。
- `table_cache`：1k/5k/10k 行，选择反馈、bordered、page/pageSize 的零 comparator/零 record copy；同 key 新文本及排序、增删和新 sort authority 失效。
- `table_reentry`：比较器更新 data/sort/page；比较器销毁 owner 并在同 index 的新 generation 挂载替代 Table。
- `table_cell_retirement`：数据 binding 或新增 cell 的同步挂载回调退役 owner。
- `table_retained`：实际 Input 草稿/session、隐藏页、数据排序变化和追加行后的外部邻居 Tab 顺序。

现有 `tree_component_tests`、`tree_review_tests`、`tree_r2_tests`、`table_component_tests` 等原始断言未改。受控拒绝、富 slot、输入隔离、原 R1/R2 生命周期、零面积、nested wheel、focus_clips 必须继续运行。

沿用 composer diagnostics、用户 comparator 和 `tests/support/allocation_probe.hpp`。仅增加私有 host 的 `TreeWork` / `TableWork`，位于 `src/component/*_component.hpp`，没有增加 `include/ryn` 诊断 API。必要性：最终 scene 相同无法证明中途是否遍历/复制了全部行。

计数口径：Tree 为整树 refresh 的 row 次数和实际 effect 呈现 row 次数；Table 为排序快照复制记录数、排序投影重建次数、呈现循环 row 次数。它们不是整个程序的总访问数；保留的 `intact` 验证、布局及 shared runtime 访问不计入这些私有计数。基准取热交互差值，不用于统计冷挂载总排序次数。

## 已执行的轻量检查

- 指定 clang-format 实际版本为 **22.1.3**。
- 对上述八个修改/新增 C++ 文件执行指定 clang-format 的 `--style=file --fallback-style=none --dry-run --Werror`，exit 0。
- 对六个 tracked 修改文件执行限定路径 `git diff --check`，exit 0；最终交接再读回 dirty 列表。
- 对四个新增文件逐个执行 `git diff --no-index --check -- NUL <file>`，均无 whitespace 诊断；exit 1 表示与空文件存在新增差异。
- 没有 configure / build / CTest / benchmark / GUI / 电脑控制，也没有读取 ROOT/out、进程监控或后台编译。
- 没有运行 `scripts/format-code.py --check`：当前脚本没有限定文件参数，且固定对全仓库启动八个 formatter worker；本轮明确仅允许限定文件检查。全局 format、OpenSpec doctor/strict 留给父线程集成阶段。

## 父线程验证待办与建议命令（本 worker 未执行）

在正式 MSVC developer environment 中使用 Ninja Multi-Config preset，build / CTest 最多两个 jobs；性能采样顺序运行，不与构建并发。先按父线程安排导入源码，再执行正式验证。

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2
cmake --build --preset windows-msvc-headless-release --parallel 2
ctest --preset windows-msvc-headless-debug --parallel 2 --output-on-failure -R 'rynui\.(state_projection_|tree_|table_|focus_|pointer_|input_|text_input_|component_failure_retirement)'
ctest --preset windows-msvc-headless-release --parallel 2 --output-on-failure -R 'rynui\.(state_projection_|tree_|table_|focus_|pointer_|input_|text_input_|component_failure_retirement)'
```

新增正式 targets 是 `rynui_state_projection_tests`、`rynui_state_projection_benchmark`；上述全构建也覆盖原 Tree/Table/TreeSelect/Focus/Input 合同。父线程按全局验收要求补完整 Debug/Release CTest，不以本交接代替验收。

`rynui_state_projection_benchmark` 接受 `tree|table count [iterations=64] [--baseline]`。每次执行先预热 8 次，再输出每次 dispatch/sync 分配次数、纳秒时间、comparator、私有访问计数及 composer rebuild；末尾 `# p95_ns` 为 dispatch+sync 总时延的 p95。没有设置不可移植的绝对时间阈值。

建议三轮 Release 记录：

```powershell
$projectionBench = '.\out\build\windows-msvc-headless\tests\Release\rynui_state_projection_benchmark.exe'
$projectionEvidence = '.\openspec\changes\102-20261029-isolate-tree-table-state-projection\evidence\benchmark'
New-Item -ItemType Directory -Force -Path $projectionEvidence | Out-Null
foreach ($round in 1..3) {
    foreach ($count in 128, 512, 1024) {
        & $projectionBench tree $count 64 > "$projectionEvidence\after-tree-$count-r$round.csv"
        if ($LASTEXITCODE) { throw "Tree benchmark failed" }
    }
    foreach ($count in 1000, 5000, 10000) {
        & $projectionBench table $count 64 > "$projectionEvidence\after-table-$count-r$round.csv"
        if ($LASTEXITCODE) { throw "Table benchmark failed" }
    }
}
```

基线须由父线程使用明确的 `dd3e71f...` 或代码等同的 `673d718...` 源码另行准备，不能用已修改二进制冒充。新测试 driver 对缺少 `.work()` 的旧私有 host 有 compile-time fallback：仍使用真实 comparator/composer 断言，基准对缺少的私有列输出 **-1（不可用）**，不能把它当零。旧基准加 `--baseline` 只取消性能门槛，便于保存真实重复工作/分配/耗时；不改变生产算法。父线程若需要完整基线 row/copy 计数，须在旧源码对应循环加等价私有计数后记录 instrumentation diff，不能推算成实测。预期旧基线 `tree_local` / `table_cache` 分别因真实 composer / comparator 重复工作失败；这一预期尚未执行验证。

最后记录源码/二进制 SHA、命令、机器、preset、三轮原始结果，并执行 OpenSpec **1.14.0** doctor/strict、全局格式和 diff 检查及独立审查。没有构建结果、基准原始数据和独立审查之前，不勾 tasks，不更新父审查文档为已修复；由父线程验证后按项目规则提交并推送。
