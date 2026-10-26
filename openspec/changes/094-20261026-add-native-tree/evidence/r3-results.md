# 094 已审共享基线集成与 Tree R2 收口证据

本记录接续 [R2 checkpoint](r2-checkpoint.md) 的待共享状态：指定已审 AC 基线已显式合并，Tree throwing-blur P2 已修复；最终定向 Debug/Release 各 62/62 通过，等待父 fresh Tree Astra R3 独立审查与 main 集成。

## 精确提交与合并处理

- worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f`；branch：`t3/9bdf1a7f`。
- merge：`d38d7971c9c39711ee944511a57f787bea3feb5b`，精确双亲为 Tree `2fd96f6209e817e892f7684c7eae6891ee576d10`、已审 AC `ad779c595a63a39d05aa787d38dccda1fcaea9c7`。实际执行 `git merge --no-ff --no-commit ad779c595a63a39d05aa787d38dccda1fcaea9c7`，解决冲突后英文 merge commit；没有 rebase/cherry-pick 或追踪 moving main。
- Tree 修复：`9120e7eab717b879123aa60ae296e62b620a65db`，parent 为上述 merge；本记录与最终日志另作独立英文证据提交，最终 HEAD/parent 在交接消息中给出。
- Theme 冲突保留 Tree 全部 38 个 IIFE 原文前缀及 200 个 ordinal，再尾追加 AC 的 1 个 IIFE、5 个 identity。双方共用旧前缀分别为 37/198。配置、override、inherit、serialize/hash/equality/dirty 与主题访问器两侧均保留。
- LayoutStyle 合并保留 AC 的 guard 先于 Scope 访问、getter 返回后再写初始值的顺序，同时保留 Tree 的完整 NodeId 检查；FocusManager 自动合并保留 Tree 消费 key 前的 keyboard modality 和 AC settled-focus 语义。窗口注册保留全部原 host、Tree、AC，以及共享 Input reset/clip/quiet retirement。CMake 保留双方全部测试注册。
- [复核脚本](verify-r3-artifacts.py) 比对已审 AC 实现、R1–R3/shared 测试、PointerRouter 和 FocusManager 声明，以及原有 Tree/Avatar 断言未丢失。未引入资源 095、其他未审源码；未修改 ROOT、Gallery、README、全局进度；无远端访问、fetch/push、PR/archive、children 或新 worktree。

## 真实 before 与 Tree 本地修复

合并共享代码后、尚未接入 Tree slot guard 时，重新编译 [shared-only build](logs/r3-shared-only-build-Debug.log)，[原 P1 before](logs/r3-shared-only-before-Debug.log) 仍真实失败（CTest exit 8）：`original=1 slot_threw=1 blur_escaped=0 blurs=1 editor_live=1 text_scenes=4 surfaces=3`。证明仅具备共享接口，或仅依赖外层挂载事务，不足以修复这个调用边界。

Tree title/icon 的用户 Content 内部现在创建局部 `RetireOnFailure`，异常传播到外围 slot/append 自动回滚之前调用共享 `retire_failed_component(full Tree ComponentId) noexcept`；正常返回后按 Tree 自己的 required 私有图及 generation 核验。外层 `MountTransaction` 和私有图损坏退役也使用该接口。没有复制 AC rows/state，也没有再修改一套共享 Input/focus/runtime/window 清理。

共享接口先以 `pointer.cancel_interaction(id, false)` 静默排空原代交互，再释放资源，正常取消仍默认通知。Tree [两项定向 after](logs/r3-slot-targeted-Debug.log) 已通过；最终两配置再次验证：

- 原 repro：带 token 的 `MountFailure` 保留，onBlur 不交付（0 次），旧 editor/interaction/session/ref 均消失。此时仅剩合法 fallback 的一个 text scene、一个 surface；最终 dispose 后全部归零。
- 新 `tree_r2_slot_retirement` 六种场景：onBlur 若执行则 throw、销毁 owner、重绑 ref；实际均未交付。另覆盖用户 slot 主动静默退役整个旧 owner 或仅旧私有 Input 后，在存活父级创建同 ref 新 Tree，再抛原异常；旧 guard 不伤新代，新 Input 实际可输入，活跃新 session 拒绝旧 stamp。
- title 与 icon、初次 mount 与已挂载父级的 `append_slot` 都覆盖；同一 Content 局部 catch 后真实 fallback 可点击。旧完整 NodeId、layout/intrinsic、editor、interaction/hit 来源、scene/glyph/surface、theme/signal 订阅与 deadline 回基线，foreign block 保留。
- 旧 R1 四项（挂载事务、零尺寸、父 Modal checkbox 单次 translation/真实 coverage、自然高度 Tree wheel handoff）及 R2 nested wheel 全部保留且通过。nested wheel 仍实际为内层 0、外层 32，含上下边界、自身 range、独立 owner 和 child/foreign popup 隔离。

## Fresh MSVC HEADLESS 定向集合

| 配置 | Tree 全部 | AC 全部 | 直接共享代表 | 去重合计 |
| --- | --- | --- | --- | --- |
| Debug | 20/20 | 23/23 | 19/19 | [62/62，34.28 秒](logs/r3-after-Debug.log) |
| Release | 20/20 | 23/23 | 19/19 | [62/62，14.81 秒](logs/r3-after-Release.log) |

Tree 20 = 原核心/模型/Theme 13 + R1 review 4 + R2 nested/throwing-blur/slot-retirement 3。AC 23 包含核心、integration、R1 review、R2 与 R3 的全部断言。其余 19 = 共享 failure retirement 1、portable focus order/state/lifecycle、pointer route、Input component/actions、Select、LayoutStyle、Modal 共 9、Popover children/focus/Escape 3、TimePicker focus/row-focus 2、DatePicker form-reset 1、Form 1、Avatar theme/lifecycle 2。

两个配置各对最终集合运行一次，共 124 次最终 CTest 执行。生成前保存 [Debug 去重清单](logs/r3-tests-Debug.json)、[Release 去重清单](logs/r3-tests-Release.json)，driver 强制检查各 62 个唯一名称，artifact 再与实际 Passed 行逐项比对。没有 skip/WILL_FAIL、无关全矩阵、Gallery/native 长循环。额外的 shared-only before 一次失败、slot 定向两次通过及两套 golden exporter 运行另列，不混入 124 次最终通过数。

工作目录始终是本 worktree。[run-r3.ps1](run-r3.ps1) 的实际四条命令：

```powershell
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r3.ps1 -Action build -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r3.ps1 -Action test -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r3.ps1 -Action build -Configuration Release
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r3.ps1 -Action test -Configuration Release
```

正式 `windows-msvc-headless` preset，Ninja Multi-Config、MSVC、HEADLESS/RECORDING；build 仅列出 29 个目标，始终 `--parallel 2`，CTest 使用同配置 preset、明确 regex、`-V --timeout 45`。[Debug build](logs/r3-build-Debug.log)、[Release build](logs/r3-build-Release.log) 均成功；补充 [slot build](logs/r3-slot-build-Debug.log)、[旧 stamp 强化断言的增量 build](logs/r3-generation-build-Debug.log) 均保留。没有自身重叠构建；Debug 测试时开始的是独立 Release 构建。沿用自身缓存与字体，仅只读锁定 ROOT/out 第三方 source；未执行 ROOT 构建程序或写 ROOT/out。原有编译警告（如 C4834、unused 参数、局部变量遮蔽）保留在日志中，未混入无关清理。

## Theme、工具与 PE

- 合并阶段 [Debug Theme build](logs/r3-merge-build-Debug.log) 成功；实际分别执行本树 `tests/Debug/rynui_tree_theme_tests.exe out/tree-ac-goldens-tree` 与 `rynui_auto_complete_theme_tests.exe out/tree-ac-goldens-ac`，见 [Tree exporter](logs/r3-goldens-tree.log)、[AC exporter](logs/r3-goldens-ac.log)。仅因两个组件组合新增而重新生成五份 golden。
- [组合比对](logs/r3-combined-theme.log) / [最终复核](logs/r3-combined-theme-final.log) / [脚本](verify-tree-ac-merge.py)：两套生产 exporter 每个文件字节完全一致；相对双方精确 parent，每种算法的各 49 个原顶层字段及所有 typed 叶值（含数值类型）不变，仅组合 identity/hash 和另一组件新增字段变化。全部检查完成后才更新受控五份 JSON。
- [format](logs/r3-format.log)：显式 clang-format 22.1.3，759 owned 文件、0 失败。
- 显式原 Node/pnpm OpenSpec 路径 [version](logs/r3-openspec-version.log) 为 1.14.0；[doctor](logs/r3-doctor.log) healthy；[strict](logs/r3-strict.log) 92 passed/0 failed；`git diff --check` 通过。
- [artifact 原始输出](logs/r3-artifacts.log) 核对 merge 双亲/祖先、双方源码与 Theme、精确 62/62 清单及 29 个目标的两个配置共 58 个 x64 PE32+；全部 stack reserve=1048576 字节，记录实际 SHA256。未加栈。
- 自身新日志仅无损转换 UTF-16/BOM 到 UTF-8，保留原文本/行尾；原 R1/R2 及 AC 历史失败日志保留，未改写为通过。

## 交接边界

本树已完成 R2 剩余项与本轮声明的 HEADLESS 验证，未执行 fresh Tree Astra R3 独立审查，未集成 main。Gallery、原生窗口/GPU、Linux、系统 IME/AT 未验收；保留原 Tree 首版排除范围。提交最终证据后交接 exact HEAD/parent 与 merge 双亲，停止等待父审查，不访问远端。
