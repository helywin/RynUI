# B3 测量源码交接（未构建、未运行）

日期：2026-10-30（Asia/Shanghai）。仅对应审计 TUNE-PERF-005/006；这是源码准备记录，不是性能或验收报告。

## 1. 身份与范围

- 工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-40136e96`。
- exact HEAD / 起点：`76c469bd5522ee64738183323fafe45c26488904`，保持未提交。
- Git branch：`t3/measure-reactive-effect-scaling`。工具准备初期实际 branch 曾为 `t3/40136e96`；交接以最终 `git branch --show-current` 为准。
- 开始设置读回：`gpt-6-astra` / `reasoningEffort=ultra` / `serviceTier=priority`；结束读回同一设置。未改设置、未创建 children 或其他 checkout。
- 未修改 `D:/code/RynUI`，未访问主树或本树的 `out`，未操作 remote/commit/push/PR/archive。
- 用户正在使用电脑；本轮未 configure/build/CTest/运行 benchmark/GUI/电脑控制/性能或进程监控。没有实测数据、二进制或已确认热点/改善。
- 103 planning 已创建并进入 apply `state=ready`，验收 `0/9`；全部 checkbox 未勾选。OpenSpec status 的 planning complete 仅代表产物齐全。

完整 dirty 文件（本次共 10 个）：

```text
tests/CMakeLists.txt
tests/reactive_fanout_benchmark.cpp
tests/effect_range_benchmark.cpp
tests/support/scaling_benchmark.hpp
openspec/changes/103-20261030-measure-reactive-effect-scaling/.openspec.yaml
openspec/changes/103-20261030-measure-reactive-effect-scaling/proposal.md
openspec/changes/103-20261030-measure-reactive-effect-scaling/design.md
openspec/changes/103-20261030-measure-reactive-effect-scaling/tasks.md
openspec/changes/103-20261030-measure-reactive-effect-scaling/specs/reactive-effect-scaling-measurement/spec.md
openspec/changes/103-20261030-measure-reactive-effect-scaling/evidence/source-handoff.md
```

生产 Reactive/renderer、101 allocation_probe、102、Tree/Table/Gallery/API/Theme/golden/资源/字体均未修改。共用新 helper 仅提供参数、时钟/分配计量和 p95，原 `tests/support/allocation_probe.hpp` 原样复用。

## 2. 源码定位与可观测边界

- `src/reactive/reactive.cpp`：`ObserverNode::run` 拆依赖，`ReactiveSource::subscribe/unsubscribe` 扫 subscribers；此路径在起点仍存在。
- `src/renderer/common/rounded_effect_gpu_resources.cpp`：`append_dirty_range` 每次追加 sort/merge；共同 `SceneResources::synchronize` 调用它。
- `src/graphics/rounded_effect.cpp`：store 的 `mark_dirty` 同样每次排序；因此 effect 的 store update 必须独立计时，不能算进 renderer planning 后归因。
- 有效计数：observer callback 实际递增、get 实际调用点、set 返回值、现有 Scheduler epoch；effect store diagnostics、GPU resource counters、Recording counters 与实际 upload callback offset/size。
- 不可观测：subscriber 扫描候选数、subscribe/unsubscribe 内部比较次数、排序比较/merge 遍历次数、纯 renderer planning 时间。CSV 对应列留空，stderr 说明 unavailable；没有用 K、K² 或 R² 填充“实测工作量”。
- 当前 API 足以生成初步扩展性 baseline，**本次不需要为了运行 harness 修改生产 instrumentation**。若父要确认具体扫描/排序归因，建议下一步最小私有计数分别放在 subscribe/unsubscribe 候选遍历与 effect dirty 合并边界，并分离规划计时；这是后续工作，未实施。相同 instrumentation 版本下才可作前后比较。

## 3. Reactive 口径

完整矩阵 K=1/64/256/1024/4096，stable/dynamic 每轮各建新 graph。默认 `--iterations 8 --warmup 2 --rounds 1`；`--smoke` 仅保留 K=1/64。

- `cold/mount` 包含创建 graph、Signal/Scope、observer 与用于校验的结果/调用次数数组。不是冷进程或冷 CPU cache；thread-local scheduler 容量可能被先前 case 预热。
- `warmup` 与 `hot` 分列，不丢弃预热样本。热数据不会重建 graph。
- stable 的每次 `active_update` 都改变 source，K 个 observer 各执行一次；每个 callback 保存 `value*3+index`，校验逐 observer 值与次数，不只比较总和。
- dynamic 的每次 iteration 按顺序执行 `switch`、`active_update`、`inactive_update`。selector 和 source 值实际改变；前两类各应触发 K 个 callback，后一类证明旧 source 已脱离，必须 0 callback。不得把 inactive 样本混入高扇出性能平均。
- 输出 source_updates 为 set 实际返回成功次数；observer_runs/source_reads 在 callback 真正执行/get 时递增；scheduler_epochs 为既有计数前后差，不是 subscriber scan。
- 每次 operation 的时钟/分配区只包住该操作与同步 flush；逐 observer 校验、样本容器预分配、CSV 输出在区外。callback 自身计数/写结果的开销属于测量。
- 末尾 scope dispose 两次，随后改两源和 selector，断言没有 callback。另有初次 callback 抛出后的订阅清理与后续正常 observer 运行检查，位于测量后。
- 复用既有 `memo_tests.cpp`（动态依赖）、`batch_tests.cpp`（批处理）、`scope_effect_tests.cpp`（生命周期及写入触发后续 epoch、最大调用深度 1）、`reactive_source_lifecycle_tests.cpp`（source 销毁/重入/queued destruction）。未复制这些测试。
- 异常新增输入只证明首次挂载 callback 抛出的传播/清理；任意 flush 中途 callback 异常、OOM、并发行为不因这条输入而被宣称全面覆盖。若后续改订阅/调度触及这些边界，必须在算法改造前补对应输入与实际证据。

CSV：`schema=1`，每个 phase/operation 含 sample 行及 summary 行。summary 的 source_updates/observer_runs/source_reads/epochs/allocations/requested_bytes/total_ns 是实际样本求和，largest_request 取最大，checksum 为最后一个样本值，mean_ns 是 total/samples，p95 是 nearest-rank `ceil(0.95*n)`。单样本行的 p95 等于自身时间，不能解读为分布。

## 4. Effect 口径

完整 `(effects,ranges)` 矩阵为 `(1000,256)`、`(10000,256)`、`(10000,1000)`；`--smoke` 只留首组，但保留全部四种模式。1000 个实例至多容纳 500 个互不相邻的单实例范围，因此不报告虚假的 `(1000,1000)` 离散样本。

- 确定性 grid，metrics 固定 `2048x2048 @ 1.0`。所有 shadow 可见，stable ids/order/数量，无窗口、字体或平台线程。
- 选中位置为 `floor(rank*N/R)`，相邻选中位置至少隔一个未动实例；更新顺序 `(step*73+19)%R`，73 与 256、1000 互质，遍历每个 rank 一次。
- `material`：opacity 在 0.5/0.75 之间切换；`geometry`：translation.x 在 0.25/0.75 之间切换；`mixed`：偶数 rank 改 geometry、奇数 rank 改 material；`overlap`：每个 rank 两者都改。全部输入为非等值且保持可见。
- `cold_sync` 仅测新 fixture 的首次共同同步；实例构造/add_batch 与 SceneResources 构造（含空 glyph sampler）在区外。首次 compact/full upload 在 cold_sync 内，不混入热样本。
- update_total_ns 单独测 store 更新，包括其验证与 dirty 累积；geometry_updates/material_updates 来自现有 diagnostics 差值，输入 ranges 来自真实 dirty spans。
- sync_total_ns 是完整 `SceneResources::synchronize`：共同 preflight/begin、空 glyph 同步、effect 同步、Recording commit。
- effect_sync_total_ns 使用既有 `SceneUploadTiming.effects - glyphs`，含 compact 检查、dirty 规划、打包、Recording enqueue/字节拥有；**不是纯 planning**。
- commit_total_ns 使用 `committed - effects`，包含 Recording commit 与事务状态更新；`planning_ns`、`sort_comparisons` 留空。
- 测量 fixture 的 RangeRecorder 将全部上传/事务调用转交既有 RecordingRenderer，仅对真实 `upload_effect_buffer` 调用记录 offset/size；ranges vector 预 reserve。Recording 的 pending 字节复制与分配属于测得同步成本，并非 GPU 时延。
- 每个成功样本核对实际 upload ranges、总 bytes、GPU/Recording counters、一次 commit、全体 logical/packed 值与未改邻居。packed 与共同 packing reference 对照，并独立检查 workload 改动的 x/opacity 字段；实际提交 buffer bytes 与 staging 比较并给出 FNV-1a hash。此 hash 是真实 bytes 指纹，不是工作量估计。
- 热样本断言无 full_upload、buffer reallocation、compaction；R 个区间必须仍是 R 个单实例上传，不能退化成 idle 或一个相邻大区间。
- 校验、hash、区间字符串构造及 CSV 输出均在计时/分配区外；这些访问会影响下一次样本 cache，数据只能按相同 harness 口径比较。
- 每轮/模式 sample 行的 `actual_ranges_first_count` 是实际回调转换得到的 `first:count;...`；summary 的该字段为空，packed_hash 是最后样本，ranges/bytes/updates/分配/时间为求和，最大分配取最大，各 p95 独立计算。时间总和不含校验间隙，不能视为整轮墙钟时间。

额外回归输入在所有计时样本之后执行：1k/256 overlap 第四次 upload 注入失败、commit 注入失败、取消后原 committed bytes 不变、旧/失败 attachment 无效、完整 retry、epoch reset 后完整恢复及无 stale release。该输入不会影响同进程后续测量。更广泛 create/upload exception/metrics/capability/owner 合同仍复用既有 `rounded_effect_gpu_resources_tests.cpp` 和 `scene_backend_tests.cpp`。

## 5. 分配、参数与输出合同

两个 exe 都单翻译单元使用既有 allocation_probe：记录被测线程 C++ new/new[]/aligned new 的请求次数、请求字节和最大请求，不是峰值 live memory、释放次数、所有 C allocator 或系统进程分配。计时记录包含探针与数据记录本身的开销。未用任何时间/分配阈值证明算法错误或优化收益。

`--iterations`/`--rounds` 范围 1..100000，`--warmup` 范围 0..100000；参数缺失、非法数字、未知选项返回非零。stdout 仅 CSV，stderr 为参数、不可观测项、成功或失败诊断；父必须核验 exit code，不能把失败前已有 CSV 当成通过。全部为 Release 有效的 require 检查，未使用 NDEBUG 会移除的 assert。

默认短样本只适合检查输入/输出。正式三轮建议每 case 64 个热样本、8 个 warmup；如高 K 耗时影响用户，父可另行预约并调整采样数量，同时保留参数与不足说明。不得选最快轮。

## 6. 父线程后续命令（本轮均未执行）

在父选定源码树、MSVC Developer 环境执行，依现有 preset/依赖配置；下面只提供命令，不要求用户手工操作。先集中构建，测量期间停止并发构建。

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2
cmake --build --preset windows-msvc-headless-release --parallel 2
ctest --preset windows-msvc-headless-debug --parallel 2
ctest --preset windows-msvc-headless-release --parallel 2
```

新增目标 `rynui_reactive_fanout_benchmark`、`rynui_effect_range_benchmark` 位于 HEADLESS return 前；后者仅在存在 `rynui_renderer_recording` 时注册（指定 HEADLESS preset 满足）。Reactive 仅链接 `rynui_reactive`；effect 链接 common/recording，不反向引入 Core renderer 依赖。HEADLESS 另复用注册 `rynui_memo_tests`、`rynui_batch_tests`、`rynui_scope_effect_tests`，只加目标不改原源码。

聚焦回归（若父需要先定位；完整 CTest 通过后无需重复）：

```powershell
ctest --preset windows-msvc-headless-debug --parallel 2 -R 'rynui\.(reactive_fanout_contract|effect_range_contract|memo|batch|scope_effect|portable\.(reactive_source_lifecycle|rounded_effect_(math|store|scene|gpu|gpu_resources)|scene_backend|backend_boundaries))$'
ctest --preset windows-msvc-headless-release --parallel 2 -R 'rynui\.(reactive_fanout_contract|effect_range_contract|memo|batch|scope_effect|portable\.(reactive_source_lifecycle|rounded_effect_(math|store|scene|gpu|gpu_resources)|scene_backend|backend_boundaries))$'
```

两个新 CTest 均调用 `--smoke --iterations 2 --warmup 1 --rounds 1`，只验证真实结果/合同，无耗时验收阈值。普通 `rynui.signal` 在现有项目的 SDL 分支含 owner-thread 测试，未在本次复制到 HEADLESS；新 fanout 覆盖当前所需单线程 Signal 通知合同。

至少三轮 Release（一次调用内部新建各轮 fixture；若要求三次独立进程，分开调用 `--rounds 1` 并记录进程/文件编号）：

```powershell
$b3Evidence = 'openspec/changes/103-20261030-measure-reactive-effect-scaling/evidence'
& './out/build/windows-msvc-headless/tests/Release/rynui_reactive_fanout_benchmark.exe' --iterations 64 --warmup 8 --rounds 3 > "$b3Evidence/reactive-baseline.csv" 2> "$b3Evidence/reactive-baseline.log"
if ($LASTEXITCODE -ne 0) { throw 'Reactive B3 measurement failed' }
& './out/build/windows-msvc-headless/tests/Release/rynui_effect_range_benchmark.exe' --iterations 64 --warmup 8 --rounds 3 > "$b3Evidence/effect-baseline.csv" 2> "$b3Evidence/effect-baseline.log"
if ($LASTEXITCODE -ne 0) { throw 'Effect B3 measurement failed' }
```

父需记录实际源码 SHA/dirty patch 身份、两 exe SHA256、preset/config、MSVC/CMake/Ninja 版本、CPU/OS、参数、时间、同机是否有干扰；没有 GPU/窗口运行，不借此声称真实 GPU/present 性能。比较前后结果必须同配置、同 dataset、同采样与 instrument 口径。

## 7. 本轮已做的轻量检查与未验项

- 显式 Node/OpenSpec 入口 `--version` 为 1.14.0；list/context/root 均指向当前隔离树。
- 当前仓库 propose/apply skills 已读取；按用户明确持续授权在规划齐全后进入 apply，CLI 提示的提交指导由用户本轮禁止提交覆盖。
- `openspec doctor --json`：healthy=true。
- `openspec validate --all --strict --no-interactive`：103 passed，0 failed。
- 指定 `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe` 确认为 22.1.3；仅格式化并 dry-run/Werror 检查两个新增 cpp 和新 helper，通过。
- `git diff --check`：通过。该命令只检查已跟踪差异；新增 C++ 由上述直接格式检查覆盖。
- 未运行仓库全量 `scripts/format-code.py --check`：该脚本固定扫描全部 tracked 源码、8 workers，且不会包含未跟踪新文件；本轮依用户“限定新增 C++、最小格式检查”使用等价 clang-format flags 定向检查。
- Context7 的 OpenSpec 文档查询报月配额耗尽；实际 workflow 以本机 1.14.0 CLI 返回 instructions 和仓库 generated skills 为准。
- 未构建/运行任何测试或 benchmark；未执行独立审查；未确认编译通过、结果通过、热点或收益。Windows MSVC HEADLESS D/R、三轮实测和后续证据决定仍全部待父执行。

父先整合本 dirty 源码并验证，再按 tasks 依赖推进。不要因本交接完成而勾选 103 或审计 B3 验收项。
