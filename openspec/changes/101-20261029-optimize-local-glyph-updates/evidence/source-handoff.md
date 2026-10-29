# 101 task 1.1 / 1.2 源码交接

## 证据边界与身份

- 状态：**source-only，未构建、未运行测试或微基准**。只准备 TUNE-PERF-001 的源码、回归和微基准；不代表 task 1.1 / 1.2 验收完成。
- 工作区：`C:/Users/jiang/.t3/worktrees/RynUI/t3-17fb4625`。所有编辑均在此处。
- 开始及结束 HEAD：`673d7181bcc116ebc36d2995cf8c8ced35b3eff2`；用户指定代码基线：`dd3e71f775fb8d4f1f46aeeff5ea444ba816e54d`。
- 开始 branch：`t3/17fb4625`；交接时实际 branch：`t3/optimize-glyph-updates`。worker 未执行任何 Git 写操作，HEAD 未变化。
- 开始及结束的运行配置读回均为 `codex / gpt-6-astra / reasoningEffort=ultra / serviceTier=priority`，未切换模型或 tier。
- OpenSpec 使用指定 Node 与固定 1.14.0 入口；已读取本树 AGENTS、apply skill，执行 status / instructions apply / list 并读取全部 contextFiles。状态 ready、任务 0/4；`tasks.md` 未改动。
- 未 configure、build、CTest、运行 benchmark、GUI、电脑控制、访问 ROOT/out 或启动后台编译。无 children、commit、push、remote、PR、archive。

## 导入文件与改动

以下 7 个路径是本次全部 dirty 文件；前 4 个为已跟踪修改，后 3 个为新增文件。没有修改父线程全局文档或 tasks。

1. `src/graphics/glyph_scene.cpp`
   - `require_range` 和索引尺寸验证保留在前；空输入/空 range 的合法 no-op 保留。
   - 非空等长分支提前到完整 replacement 构造之前。先 `geometry_dirty_ranges_.reserve_for_append()`，再对目标范围执行一次 `std::memmove(..., instances.size_bytes())`，最后追加原 range 并返回。
   - `GlyphInstance` 的 trivially-copyable 静态断言保护字节复制；完全自引用、左右部分 overlap 不需要额外 glyph 临时 vector。
   - 保留原 material dirty，geometry dirty 按既有累积器合并；准备失败发生在 retained glyph 写入前。
   - 零长度插入的 alias-safe 分支保留；变长 prefix/input/suffix replacement、swap、shifted dirty 规则保持，仅去掉已经前移的等长分支。
2. `tests/CMakeLists.txt`
   - 在 HEADLESS 的 `add_subdirectory(portable)` / `return()` 之前注册 `rynui_glyph_replace_tests` 和 `rynui_glyph_replace_benchmark`，链接既有 `rynui_graphics` / `rynui_text`。
   - 新 CTest 为 `rynui.glyph_replace`；微基准单独手动运行，不加入全套 CTest 的耗时测量。
3. `tests/support/allocation_probe.hpp`
   - 复用现有全局 new / aligned new failpoint，新增 requested bytes / largest request 统计；begin 重置新计数。
   - 原 count / fail_at 行为保持；未加生产诊断 API 或通用 allocator。
4. `tests/text_scene_service_tests.cpp`
   - 新增真实 TextScene `1000 -> 1001` 内容更新回归，断言目标实际改变、store 地址稳定、前后邻居内容和 instance/draw/line ranges 不变、dirty 精确、ordered commands 与 compaction 计数保持。
   - 原有断言全部保留。
5. `tests/glyph_replace_tests.cpp`
   - 1k / 10k / 100k store 的外部 4 glyph 替换：完整内容对比、地址稳定、冷路径有界 metadata 分配、热路径 failpoint 下零分配、精确 dirty。
   - 完全 self alias、左右部分 overlap、左右不相交 self span；保留 source 原值后比较全部邻居。
   - 保留既有 material 与 geometry dirty 邻居，覆盖相邻合并、无序插入与未修改间隙。
   - 空 store、首/中/尾空 range、非法/溢出 range，外部及空输入均拒绝且保持原状态。
   - 对真实 dirty-vector 分配注入 `bad_alloc`，覆盖外部 / self / 左右 overlap，分别测试空 geometry dirty 和带已有 geometry dirty；验证失败前完整内容、地址、两层 dirty 保持以及重试。动态寻找非空 dirty 的增长边界，不依赖 vector 的具体 capacity 增长点。
6. `tests/glyph_replace_benchmark.cpp`
   - 默认 `--iterations 2000 --rounds 3`，每轮 1k / 10k / 100k × external / self / overlap-left / overlap-right 共 12 组。
   - 记录冷/热 new 请求次数、请求字节、最大请求、store 地址变化、dirty first/count/bytes、replace-only 总耗时、mean / p50 / p95。
   - 每次操作检查源快照与目标、range、dirty；最后检查全部未修改邻居。正式 local 模式要求冷路径没有 store 重建、热路径零分配及地址稳定。
   - 提供 `--baseline` 供父线程把同一基准源码放到固定旧实现上测量；该模式要求实际观测到全 store 分配及地址变化，否则拒绝，以免在新源码上误标旧复制模型。
7. `openspec/changes/101-20261029-optimize-local-glyph-updates/evidence/source-handoff.md`

## 复制量统计口径与调用链审阅

allocation 字节来自真实 operator new 请求；dirty 字节来自 `store.bytes(target)`。`copy_bytes_source_upper_bound_*` 明确是**源码推导上界，非运行时 memcpy/memmove 计数**，不混充实测结果：

- 固定旧实现执行 prefix + 4 glyph input + suffix 三段 insert，总 glyph 复制范围为整个 store。
- 新等长路径只有一次长度 `4 * sizeof(GlyphInstance)` 的 memmove，未复制 prefix/suffix，未创建 glyph 临时 vector；完全 self alias 的实际底层移动量可能更少。dirty 元数据移动独立于这里的 glyph 复制字节口径。
- 地址稳定和真实 allocation 计数证明的是存储/分配行为；没有把“少一个 malloc”单独当作复制局部化证据。本轮没有填任何耗时或分配实测结果。

已审阅 `TextSceneService::synchronize()` 的旧 range / `replace_text()` / offset / `remap_following()` 链路，以及删除时 `replace(range,{})` 和 `shift_primitive()` 对 instance/draw/line ranges 的偏移规则。`GlyphScene::replace_text()` 以 pending glyph vector 调用本次修复；内容或位置重建均经过等长分支。offset 为 0 时 remap 立即返回；变长删除/增长仍走原算法。本次不改 `src/text/text_scene_service.cpp`、renderer 或 101 task 2.1 / 2.2。

## 已执行的轻量检查

- 指定 clang-format 实际版本 `22.1.3`。
- 仅对上述 5 个修改/新增 C++/hpp 文件执行 `--style=file --fallback-style=none` 格式化与 `--dry-run --Werror`，返回 0。
- `git diff --check` 返回 0。
- 新增文件以 `git diff --no-index --check -- NUL <file>` 检查，无空白错误诊断；no-index 返回 1 表示文件与空输入有差异，不是测试通过状态。
- 未运行仓库全量 `scripts/format-code.py --check`（该脚本没有文件过滤选项且会并行扫描全部源码）；父线程正式收口时运行。

## 父线程验证待办与建议命令（均未执行）

由父线程导入确切 diff 后，在 UTF-8 的 VS Developer PowerShell 中按正式 MSVC / Ninja Multi-Config HEADLESS 流程串行调度；build 最多 2 jobs。首先保留旧源码失败证据，再验证本补丁。建议首批 targets：

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_glyph_replace_tests rynui_glyph_replace_benchmark rynui_portable_glyph_scene rynui_portable_text_scene_service
ctest --preset windows-msvc-headless-debug --no-tests=error -R '^(rynui\.glyph_replace|rynui\.portable\.(glyph_scene|text_scene_service))$'
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_glyph_replace_tests rynui_glyph_replace_benchmark rynui_portable_glyph_scene rynui_portable_text_scene_service
ctest --preset windows-msvc-headless-release --no-tests=error -R '^(rynui\.glyph_replace|rynui\.portable\.(glyph_scene|text_scene_service))$'
```

当前 local 源码的三轮计时（Debug / Release 分别留原始日志，无并发构建）：

```powershell
& '.\out\build\windows-msvc-headless\tests\Debug\rynui_glyph_replace_benchmark.exe' --iterations 2000 --rounds 3
& '.\out\build\windows-msvc-headless\tests\Release\rynui_glyph_replace_benchmark.exe' --iterations 2000 --rounds 3
```

旧版本对照仅使用父线程准备的固定旧源码与同一新 benchmark / probe / CMake 注册；保留构建源码身份与配置，在同等参数末尾增加 `--baseline`。不要在新实现的正式验证命令中使用该参数。旧版局部性回归预期失败属于待复现结果，本 worker 未运行，不能当作已取得失败证据。

task 2.2 的 main 集成仍由父线程完成：完整受影响 Text / Input / renderer 合同与全套 HEADLESS D/R。共享 allocation probe 的原调用者（Text editor/history/reconcile/caret、bidi、Input、reactive、hit-test 等）应随正式测试一起验证；新计数不改变原 fail_at/count 合同。最后执行全量 format、OpenSpec 1.14 doctor / strict validate、diff check、独立审查，按父线程提交/推送规则收口。本 worker 未勾选任何任务，未记录任何功能或性能验收通过。
