# Astra R2 P2：行 input block 的资源生命周期

## Baseline / Reopen

基线为 `5f71281a04c130949c33bc23506d263d7f6957c0`，同一 worktree / branch。收到 R2 后，将 080 tasks 2.1/3.1/4.1 重新打开；本阶段新回归通过后仍保持打开，等待受影响检查与最终校验。没有合并 main、TimePicker、Gallery 或其他 WIP。

## BEFORE：实际失败回归

先新增 `navigation_block_lifetime_tests.cpp` 与 10 个 CTest cases，两个生产 host 保持基线不变。每个 case 保持同一个 WindowComponentServices、ComponentHost 和 InteractionRegistry，完成 8 次实际 append/remount → native Button/Input 使用 → blocked teardown：

- Breadcrumb / Steps，各覆盖 hidden、disabled、inactive、hidden_blur、disabled_blur。
- hidden_blur / disabled_blur 在真实 Input onBlur 内销毁 owner，并在回调中确认 owned row block 已注册。
- 保留三个存活的原生 interaction，以及一个由其他 owner 注册的 block，作为非零基线。
- 直接只读检查现有 `blocked_roots_` 集合：test TU 通过显式模板实例化取得真实成员指针，没有生产 accessor、registry header 修改或内部 layout 的副本/cast。
- 同时检查真实 HitTest、pointer onClick、Focus、TextCommitted/composition、存活编辑器、component/node/effect/live interaction 数量，以及不误清理其他 owner。

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_navigation_block_lifetime_tests --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui.navigation_block_lifetime_' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_navigation_block_lifetime_tests --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui.navigation_block_lifetime_' --output-on-failure
```

修复前 Debug / Release 均 **0/10 passed，10/10 failed**。所有场景先完成 8 轮再报告注册集合不回基线，而非首轮即停止：

| 两组件相同结果 | 第 8 轮 mounted | live records / baseline | blocks / baseline |
| --- | --- | --- | --- |
| hidden、disabled、hidden_blur、disabled_blur | 0 | 3 / 3 | 9 / 1 |
| inactive（两个 row 均被屏蔽） | 0 | 3 / 3 | 17 / 1 |

实际每轮多留 1 或 2 个 stale ComponentId；其他资源已回基线。原始 ignored 日志为 `out/080-r2-repro-{debug,release}.log`。

## Fix / Order / Ownership

仅修改两个 host 的行 resource cleanup：

1. 按值捕获该行 ComponentId，沿用已存在且覆盖整个 component resource 生命周期的 WindowComponentServices owner 引用。
2. 与 Spin 的既有 cleanup 模式相同，直接调用 `services.interactions().set_input_blocked(row_component, false)`，再执行原有 pointer/focus/interaction/scene/surface/layout 清理。
3. 不通过 `WindowComponentServices::set_input_blocked`，避免资源拆卸中触发输入重协调；外层 destroy 完成后按已有路径处理 revision。ComponentHost 已先 dispose 所有 scope，cleanup 回调不能经 refresh 重注册 block；state/node/registry 此时仍有效。
4. 只注销精确的行 ID（包含 generation），不清空全局集合、不取消父边界或其他 owner，不修改 InteractionRegistry 或通用 Input/窗口实现。

## AFTER：新回归

同一组官方 windows-msvc-headless / MSVC 19.51 / Ninja Multi-Config 命令，build 上限 4：

- Debug **10/10**，1.28 秒。
- Release **10/10**，0.58 秒。
- 每个配置均完成 10 个场景 × 8 轮同窗口循环。每轮 mounted=0、records=3/3、blocks=1/1，且完整 block ID 集合等于基线。
- 其他 owner 的 block 始终有效，最后由其调用者显式解除并恢复实际点击；无 stale gate 影响存活 Input 或 remount 子控件。blur 只交付一次；component/node/effect/interaction 数量回基线。
- 原始成功 stdout 保存在 ignored `out/080-r2-fix-{debug,release}-details.log`；CTest 摘要为 `out/080-r2-fix-{debug,release}.log`。

本阶段只记录新回归和最小修复；既有 R1 child pointer/native input 与实际四边 focus coverage 测试保持原文，后续只执行受影响集合一次。没有新生产诊断 API、registry redesign、Theme 编辑或 stack 变更。独立 Astra R3、Gallery、原生 GPU/Linux、系统 IME、辅助技术未验收。
