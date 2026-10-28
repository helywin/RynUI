# 094 Fresh Astra R2 部分修复 checkpoint

R2 **尚未完成**。nested Tree wheel 已独立修复；focused Input 的 throwing-blur 挂载回滚仍失败，等待父提供通过 R4 审查的共享退役接口和兼容基线。此记录不代表整体验收通过。

## 本地提交与边界

- worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f`，branch：`t3/9bdf1a7f`。
- 起点：`b4b8c0ceeceb4b5e24aa85298a7219b5c41dfa3b`（parent `243abf57fa40311146aa4584d47967727edb3caf`）。
- `1ee42c1d`：仅 R2 两项回归、真实 before 日志与既有 tasks；生产代码未变。
- `f22ae6222c7bd7a86f7d1cc02e3c4dc3b4eeaed6`：Tree 自身 wheel handoff；不含共享清理修改。
- 本记录、验证 driver、日志与保留合同另作证据提交；最终 HEAD/parent 在交接消息中给出。无远端访问/fetch/push、PR/archive、新 worktree 或 children；未修改 ROOT、Gallery、README、全局进度或导入 AC WIP。

## 两项真实 before

[最终准确 before](logs/r2-before-Debug.log) 在未修复生产代码上新编译 Debug 执行两项，2/2 均失败，CTest exit 8：

- `tree_r2_nested_wheel`：确认真实 hit 属于 richTitle 中的内 Tree；内层 height=24、仅一行无 range，外层 height=80 且 overflow。实际 wheel 后 `inner=0 outer=0 independent=0`，未交还逻辑祖先。
- `tree_r2_throwing_blur_rollback`：富 slot 挂载真实 Input，使用 ref 在初次布局前聚焦并建立 preedit，再抛出带 token 的原 mount 异常；Input.onBlur 自身抛出另一种异常。**同一 Content 内局部 catch 后继续挂载合法 fallback Button**。实际 `original=1 slot_threw=1 blur_escaped=0 blurs=1 editor_live=1 interaction_live=0 text_scenes=4 surfaces=3 session=0 ref=0`。原异常保留，但 editor 未释放；scene/surface 数字为当时的总量，包含合法 fallback，不把全部总量都称为泄漏。测试据旧 editor 身份仍存在而失败，不能视为完整回滚。

保留 [初版 before](logs/r2-before-Debug-initial.log)：其中 `exception_ptr` 地址相等判定不能在本机正确证明原异常身份，已改为异常类型加 payload token；最终准确 before 的 `original=1` 才是结论。[初版 fixture build](logs/r2-build-Debug-fixture.log) 包含 CMake 自动重配和误用 `vertical()` 的编译失败，改为真实 API `vertical(true)` 后 [before build](logs/r2-build-Debug-before.log) 成功。以上 fixture 问题不计为产品 finding。

## 独立 wheel 修复及定向结果

最近 Tree 没有 range 或已到该方向边界时，继续遍历逻辑 parent，找到可消费的 Tree 才更新 scroll；每次仍经过原 `is_window_layer_root` 检查，不跨最近 popup/window layer。inactive 或 clip 不接纳的 Tree 仍终止该路径，未扫描无关 Tree owner。

[单项 after](logs/r2-wheel-Debug.log) 确认 `inner=0 outer=32 independent=0`，并实际验证：内层有 range 自己滚；上边界交还使外层归零；下边界交还使外层增加；独立 sibling Tree 只改自己的 scroll；child 与 foreign Select popup 滚动时三棵 Tree 均不动。

| 最终定向配置 | nested wheel | 旧 R1 四项 | Tree 直接代表 5 项 | 共享代表 4 项 | throwing blur | 总计 |
| --- | --- | --- | --- | --- | --- | --- |
| Debug | 1 通过 | 4 通过 | 5 通过 | 4 通过 | **1 失败** | [14/15，10.74 秒](logs/r2-after-Debug.log) |
| Release | 1 通过 | 4 通过 | 5 通过 | 4 通过 | **1 失败** | [14/15，5.18 秒](logs/r2-after-Release.log) |

每配置对这个 15 项集合运行一次，两个配置共 30 次最终执行、28 通过、2 个相同未解决共享失败；Debug before 与单项 after 另计。原有断言未删改，未设 `WILL_FAIL`、skip 或屏蔽失败。Tree 代表为 rich/scroll/boundaries/mount_lifecycle/lifecycle；共享代表为 Input/Select/Modal/Focus lifecycle。没有重跑无关矩阵或重新生成金样。

工作目录为本树，正式 MSVC `windows-msvc-headless`、Ninja Multi-Config。实际命令如下，driver 中 build 显式列出七个目标、始终 `--parallel 2`，CTest 使用对应 preset、`-V --timeout 45`：

```powershell
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r2.ps1 -Action build -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r2.ps1 -Action test -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r2.ps1 -Action build -Configuration Release
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r2.ps1 -Action test -Configuration Release
```

[Debug build](logs/r2-build-Debug.log)、[Release build](logs/r2-build-Release.log) 均成功，test driver 均返回非零。各轮顺序执行，没有自身重叠构建。CMake 仅复用原锁定第三方 source，未改 ROOT/out；缓存、字体、构建、程序及日志均在自身树。

## 未解决共享接口与下一步

当前基线没有 `retire_failed_component`。现有 `FocusManager::cancel_interaction` 会交付用户 blur；`PointerRouter::cancel_pointer_interaction` 仅避免再取消 focus，仍可能交付 pointer handler。底层不通知的取消分支为私有实现，无法从 Tree 直接复用为完整静音退役。未通过清空 handler、逐个删除 Input 私有资源等方式另写第二套 shared cleanup。

父已告知 AC 的 `4c67fd5c` 及相关 `40bba184`/`648da9b6`/`ad779c59`，同时明确存在 AC R2 基础依赖且 R4 尚待确认。本树未 cherry-pick、merge 或读取这些 WIP 作为已验证能力。后续需要父给出可用于当前 Tree 基线的 verified SHA/接口组合，合同是：

1. `WindowComponentServices::retire_failed_component(full ComponentId) noexcept`（按父提供的实际接口接入）先静默取消原代 interaction 及后代，再完整释放原代 Input/editor/scene/surface/layout/订阅/owned blocks；正常取消流程仍默认通知。
2. 在 Tree 调用用户 richTitle/icon slot 的**内部局部 try/RAII** 中，捕获原完整 owner id 并在异常向外传播、`ComponentHost::mount_slot/append_slot` 自身回滚子树**之前**调用。当前外层 `MountTransaction` 此时已太晚，单替换外层 destroy 调用不够。
3. 原异常继续重抛，静音退役不交付 throwing blur；旧 owner 已失效或回调已创建新 generation 时必须仅处理原完整 id，不误删同 index 的新 owner、同 ref 新绑定或 foreign owner。
4. 共享接口接入后再验证本 repro 的旧 editor 消失、全部资源回基线、fallback 真实可用及新代隔离；task 7.4 保持未勾选，不能据 wheel 通过声称 R2 全部完成。

## 检查与证据边界

- [format](logs/r2-format.log)：clang-format 22.1.3，744 owned 文件、0 失败。
- 显式原 Node/pnpm OpenSpec 路径 [version](logs/r2-openspec-version.log) 为 1.14.0；[doctor](logs/r2-doctor.log) healthy；[strict](logs/r2-strict.log) 91 passed/0 failed；`git diff --check` 通过。
- [保留合同](logs/r2-retained-theme.log)：旧 37 IIFE、198 ordinal、240 个旧 typed 字段保留。脚本原输出词 “fresh goldens” 在本轮指传入的现有受控五份 golden，未重新生成。
- [checkpoint audit](logs/r2-artifacts.log) / [复核脚本](verify-r2-checkpoint.py)：确认每配置 14 通过且唯一真实失败为 throwing blur、旧 R1 四项通过；确认共享 Input/focus/runtime/WindowServices、原有测试、公开 API/Theme/presets/五 golden 相对 b4b8c0ce 未改；记录七个目标的两个配置共 14 个 x64 PE32+ SHA256，stack reserve 全为 1048576 字节。
- 新日志仅做 UTF-16/BOM 到 UTF-8 的无损文本编码转换，保留行尾及原诊断。工作区提交后无未保存改动仅表示 Git 状态，不表示 R2 验收通过。main/Gallery、原生窗口/GPU、Linux、系统 IME/AT 均未验收。
