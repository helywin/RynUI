# Astra R2 inactive converted fill 修复

## 基点与范围

2026-10-25，从干净的 `d25c3ef42a594e2f9ceec9f218cfaa27331f6f67` 开始，原 worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-f0b94dd3`、分支 `t3/native-message-notification`。只处理 R2 指定的缓存停更问题，重开 085 的 2.3、2.4、3.1；R1 fill、trim、anchor 的原断言保留。

没有新功能、change、children、PR 或 archive；没有操作 ROOT、Git 远端或 push，也没有导入 087/WIP。Theme/goldens、renderer/GPU ABI、栈配置和 clock/trim 均未改。

## 修复前实测

先只在现有 review fixture/CMake 中新增两个用例，生产代码保持 d25c3ef4：

- `notice_review_inactive_surface`：普通 root 的独立受控 open=true Select 与 Notification 同时显示；关闭最后 Notification，确认 Select 仍 open，改变 controlled value。实际 ordered scene 的新 selected background coverage 为 **0**，断言失败。
- `notice_review_inactive_content`：普通 root 的独立受控 Popconfirm 内挂载真实 Slider dots，另开 Notification；关闭最后 Notification 后修改 Slider value。实际 ordered scene 的新 dot selected color coverage 为 **0**，断言失败。

两个有效 repro 在正式 MSVC headless **Debug 0/2 passed、Release 0/2 passed**，都在修改生产代码前编译并运行。失败输出分别为 `inactive Select selected coverage=0`、`inactive Slider dot selected coverage=0`。

## 生产修复与合并注意点

唯一生产路径变更是 `src/component/retained_surface_service.cpp` 中的 **RetainedSurfaceService::compact_effects**（本树约 603 行）：

1. enable=true 时继续调用原 `synchronize_clipped_fills`，不改变准入规则。
2. enable=false 时遍历 surface 与 content-range records，移除并清空所有 `clipped_fill_ids`，标记 topology/draw binding 已变化。
3. surface 还重新同步调用方保存的 `record.effects`，撤销转换期间注入的 decoration clip；即使之前 `update_effects` 因 source 相等而 no-op，也不会保留旧 effective clip。
4. 沿用后续 effect compaction、`bind_fragment` 和 `publish_content`，恢复 source quad commands 并返回变化，使窗口服务重建实际 ordered scene。后续 material/geometry 更新直接作用于源 quads。

**与 087 合并：**本轮没有修改 WindowComponentServices 的 enable 条件，也没有修改 `synchronize_clipped_fills` 两处准入谓词（R1 原约 492/552 行）。父可保留 087 在这两处的必要修改，只合入本轮 compact_effects 的关闭/释放分支；不要整文件覆盖。调用方认为仍需转换时传 true、沿用相应准入规则；关闭转换时必须保留两类缓存释放、surface effects 恢复和 draw 重绑这三步。

## 修复后证据

使用本 worktree 的 MSVC Ninja Multi-Config `windows-msvc-headless` presets，最多 4 jobs。构建目标仍为 `rynui_notice_review_tests`、`rynui_notice_component_tests`、`rynui_portable_modal_component`、`rynui_portable_button_scene_service` 及正式依赖。

同一必要去重集合分别在 Debug/Release 运行：

```text
ctest --preset windows-msvc-headless-debug -R '^rynui\.(notice_review_.*|notice_(retention|clock|lifecycle|boundaries|popup|clipping)|portable\.(modal_component|button_scene_service))$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.(notice_review_.*|notice_(retention|clock|lifecycle|boundaries|popup|clipping)|portable\.(modal_component|button_scene_service))$' --output-on-failure
```

- Debug：**16/16 passed**，5.45 秒。
- Release：**16/16 passed**，3.12 秒。
- 原 13 项与 anchor 共 14 项断言不变，全部通过；仅增加上述两个 R2 用例，没有扩大测试平台或运行历史全套。
- Select 新 selected background 与 Slider dot 新 selected color coverage 均从 **0 → 1**；旧颜色在旧选中位置消失。
- 随后分别移动独立 Select/Popconfirm 的布局位置：新位置 coverage 正确，旧位置 coverage 为 0，证明不是只修 material 而留下旧 geometry。
- 最后 notice 确实已不可见，独立 popup 仍 open；关闭转换后 effects live count 恢复显示 notice 前的基线。最终 dispose 后 surfaces size 与 rounded effects live count 均为 0。
- 原 normal-flow Button 源路径、Modal、子 popup clip boundary/wheel/Escape、focus decoration/input、foreign blocks、R1 fill/trim 和 anchor 验证均通过。
- 未使用隐藏 notice、额外 tick、无条件请求帧或新 renderer 路径。

## 收口检查

clang-format **22.1.3** 的全 owned-source check 通过（689 files / 0 failures）；显式 pnpm **OpenSpec 1.14.0** doctor healthy、all strict **84/84 passed**；`git diff --check` 通过。

上述是 Windows MSVC headless 的逻辑场景、实际 ordered draw coverage 和资源生命周期证据，不代替 GPU/native/Linux/system IME/AT 验收。完成当前必要修复后停止；独立 Astra R3 与主分支集成由父负责。
