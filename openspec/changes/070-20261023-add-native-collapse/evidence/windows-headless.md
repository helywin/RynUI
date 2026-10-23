# Collapse 平台通用验收

首版记录保留如下；2026-10-24 的 Astra R1/R2 复现、修复和后续验收见文末独立章节。

## 实际环境与范围

- 日期：2026-10-23；实际主机 `Microsoft Windows NT 10.0.26300.0`。
- 隔离工作区：`C:/Users/jiang/.t3/worktrees/RynUI/t3-3033c921`，起点 `69689046bc9a99b94c98086cb3c849b79015acbc`。
- 被验实现提交：`711800ba82c478f4db979ac18f21b3609b87781b`；后续收口只更新文档与任务状态。
- 正式 `windows-msvc-headless` preset，Ninja Multi-Config，MSVC 19.51.36256.0，x64，Debug/Release。
- 本工作区独立 `out/build/windows-msvc-headless`；BUNDLED 锁定依赖、验证字体，编译和 CTest 均最多 4 jobs。未使用或修改根工作区构建目录，仅只读查看其 MSVC 环境脚本。
- Ant Design 6.6.5 的源码 commit 与 Collapse style SHA256 核对见 [design.md](../design.md)，没有升级第三方基线。

## 实跑结果

- Debug focused CTest：**18/18 通过**，5.20 秒。
- Release focused CTest：**18/18 通过**，2.23 秒。
- 新增 `collapse_public_api`、`collapse_theme`、`collapse_component`；共同回归包含 Theme algorithm/runtime、Tabs theme/component、Modal theme/boundary/component、Drawer theme/component、Popconfirm theme/component、Tag/Badge theme、feedback theme/component，以及 portable backend boundaries。
- 五份 Theme golden 由现有 `rynui_portable_theme_algorithm --write-goldens` 入口生成，并通过 JSON 比较确认：与起点相比，所有既有字段值不变，仅追加 `collapse` 数据及更新 `identity`。既有 TokenIdentity 枚举值保留，新分组尾部追加。
- `python scripts/format-code.py --check` 使用 clang-format 22.1.3：537 个自有源文件，0 failures。
- `openspec doctor --json` 健康，strict validate **67/67**，`git diff --check` 通过。

## 核心合同证据

`tests/collapse_component_tests.cpp` 实际驱动生产 WindowComponentServices 和文本编辑器，验证：

- 受控 owner 拒绝/同步接受建议 keys；多开、accordion 至多一个及全部关闭；未知/重复 key 归一化，显式 disabled active key 仍可展示。
- 真正 Input 编辑后的草稿与 editor identity 保留；隐藏时 sibling 的 y 减少真实 body height；session、InputRef、迟到文本事件与文本选择 pointer capture 均退出。
- 正文 Select 浮层隐藏后不在重开时恢复；所有 slot 只挂载一次；销毁清理 descendant interactions 与 effects。
- 独立 header Tab stop、方向/Home/End 焦点导航、Enter/Space 切换；disabled header 不可操作，extra 原生 Button 的 pointer/Enter 独立可用；disable-during-capture 不激活。
- 实际 Modal 和 Spin 共同逻辑边界阻止外部 header pointer/focus/keyboard，Modal 内部 header 可操作，解除边界后恢复。
- 真实保留 Icon glyph 的右向/下向状态；三种尺寸；Theme metrics 原地更新；纯颜色只更新 material、不增加根 measure_count；无变化时无 frame/deadline/effect 更新。
- controlled/uncontrolled onChange 销毁，以及隐藏正文 onBlur 销毁，不留下 editor/session/interaction/effect。
- 空/重复 key、active/default 冲突和非法尺寸在执行 slot 前拒绝；独立 Theme 继承/reset/算法、订阅分组、非法 override 原子回滚通过。

## 重现入口

先进入 MSVC x64 Developer Environment，在本 worktree 执行 `cmake --preset windows-msvc-headless`。分别构建以下 targets：

```powershell
$targets = @(
  'rynui_collapse_public_api_tests', 'rynui_collapse_theme_tests', 'rynui_collapse_component_tests',
  'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime',
  'rynui_portable_tabs_theme', 'rynui_portable_tabs_component',
  'rynui_portable_modal_theme', 'rynui_portable_modal_boundary', 'rynui_portable_modal_component',
  'rynui_portable_drawer_theme', 'rynui_portable_drawer_component',
  'rynui_portable_popconfirm_theme', 'rynui_portable_popconfirm_component',
  'rynui_tag_badge_theme_tests', 'rynui_feedback_theme_tests', 'rynui_feedback_component_tests'
)
$filter = 'rynui\.(collapse_.*|feedback_(theme|component)|tag_badge_theme|portable\.(theme_(algorithm|runtime)|tabs_(component|theme)|modal_(component|boundary|theme)|drawer_(component|theme)|popconfirm_(component|theme)|backend_boundaries))$'
foreach ($config in @('debug', 'release')) {
  cmake --build --preset "windows-msvc-headless-$config" --parallel 4 --target $targets
  if ($LASTEXITCODE -ne 0) { throw 'build failed' }
  ctest --preset "windows-msvc-headless-$config" --parallel 4 -R $filter --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }
}
```

## 明确未声称的结果

这是平台通用合同在 Windows MSVC 上的实际 headless 证据，不是全仓 CTest、GPU、Linux、真实窗口视觉或人工 IME 验收。首版不含 motion、动态 panel topology/reconciliation、drag、服务、RTL/辅助功能桥接或完整 Web aliases；详细公共边界见 [Collapse API](../../../../docs/collapse.md)。未修改 Gallery、README、组件完成表或 support overlay，未合并 Table/Menu/InputNumber，未 push、PR、archive 或创建子任务。

## 2026-10-24 Astra R1：三项 P2 均已实跑复现并修复

本轮从干净的 `389bc8841e5ed3f529190d8c77a4e777dc9cca85` 继续，仍使用同一隔离工作区、独立构建目录及 `windows-msvc-headless` MSVC x64/Ninja Multi-Config preset，最多 4 编译 jobs。没有合并根 main（包括后续 Table/Menu/InputNumber 和 Theme binding bridge）。上方 `711800b` 与 18/18 只表示首版历史验收，本节记录本轮结果。

### 原实现上的实际失败

先只增加 regression tests，再编译原实现，分别直接运行 Debug `rynui_collapse_component_tests.exe eligibility|clipping|popup`，三项各自失败：

```text
eligibility: enabled header retained stale pointer eligibility
clipping: constrained header glyph painted into sibling
popup: unfocused hidden popup resurrected on placeholder update
```

这些是本轮实际运行结果，不再仅依赖审查时的 source trace。新增用例保留三个命令行入口，默认 component test 同样执行全部用例。

### 修复与对应合同

1. **HitTest 缓存失效**：`set_eligible` 真正改变 header eligibility 时，对该 header 追加 `HitTest` dirty。测试从 `disabled=true` 开始，重复两轮 enable/sync/真实 pointer click、键盘关闭、disable/sync/pointer 不激活；单次 eligibility 更新不增加 header measure_count。
2. **受限 header 绘制**：在居中前夹紧 arrow/title/extra 尺寸，并为 Collapse、header 及逻辑 sibling extra 设置现有 descendant clip。测试两个关闭的 Middle panel，在高度 `48/55/32/0/48` 下保留原 slot，检查 following sibling 的实际 y、arrow/title/extra bounds、ordered scene 实际 glyph 的 clip 交集与实际 mark 背景 quad；覆盖第二个 header 剩余 0px 和 7px，未用“节点高度已夹紧”替代绘制验证。
3. **未聚焦 popup 清理**：Select 的 pre-layout hook 识别 inactive owner 上仍 open 且未 suppressed 的 popup，走既有 `on_input_boundary_changed` 关闭/suppression/close intent 流程；判断不再依赖 `visible` 或 blur。uncontrolled open 被清除，controlled parent 拒绝关闭时保持其值并持续 suppressed，显式重新打开仍可用。覆盖未聚焦 `defaultOpen(true)` 和受控 `open=true`、三轮 Collapse 隐藏/重开/placeholder 更新、Modal/Spin 一致性，以及 close callback 销毁整个 Collapse 后零遗留 interactions/editors。

### 最新实际结果

- Debug affected CTest：**21/21 通过**，5.06 秒。
- Release affected CTest：**21/21 通过**，2.89 秒。
- 在首版 18 项的基础上增加既有 `portable.select_component`、`portable.card_component`、`portable.tooltip_component` 回归，覆盖共享 Select hook 与裁剪；Modal/Spin、Tabs、Popconfirm、Theme 与 backend boundary 回归保留。
- clang-format 22.1.3，`python scripts/format-code.py --check`：**537 个文件，0 failures**。
- `openspec doctor --json` 健康；`openspec validate --all --strict --no-interactive` **67/67 通过**；`git diff --check` 通过。
- 本轮不改变 Theme members、subscriptions、goldens、renderer 或 scene ABI；仅本地英文提交，不改 Gallery 或全局文档，不 push/PR/archive/创建子任务。不增加 GPU/Linux/人工 IME 声明。

重现最新 affected suite：沿用上面的 `$targets`，追加三个既有 targets，并用以下 filter 替换原 filter，然后分别执行 Debug/Release build/CTest；`--parallel 4` 保持不变。

```powershell
$targets += @('rynui_portable_select_component', 'rynui_portable_card_component', 'rynui_portable_tooltip_component')
$filter = 'rynui\.(collapse_.*|feedback_(theme|component)|tag_badge_theme|portable\.(theme_(algorithm|runtime)|tabs_(component|theme)|modal_(component|boundary|theme)|drawer_(component|theme)|popconfirm_(component|theme)|select_component|card_component|tooltip_component|backend_boundaries))$'
```

## 2026-10-24 Astra R2：两项焦点 P2 已复现并修复

从干净的 `815a509dc468628145030ca96f2fef2c65bae9c8` 继续，没有合并根分支。本轮唯一修改的生产源为 `collapse_component.cpp`，保持原有 MSVC x64/Ninja Multi-Config `windows-msvc-headless`、独立构建目录及最多 4 jobs。

先仅新增测试，两个独立 Debug 入口均在原实现上退出 1：

```text
extra-focus: extra Button focus ring clipped by tight content bounds
keyboard-focus: pointer-to-keyboard header activation did not paint focus ring
```

- **extra 焦点环**：仅在存在 extra 时增加被动 header viewport，仍是 Collapse 下的独立逻辑 sibling；内容保持原尺寸与位置，descendant clip 改为可用 header rectangle。测试用真实 Tab 聚焦原生 Button，核对 width=3/offset=1 的实际 rounded effect；去除 clip 后求完整 outline bounds，确认原 clip 完整包含它，并检查控制外侧采样点位于 clip 内且 coverage > 0.5。覆盖 Middle/Small/Large、disabled header 下独立 pointer/Enter 激活以及 slot 只挂载一次；R1 的短/零高度实际 glyph/quad 裁剪测试保留并通过。
- **pointer 转 keyboard**：header 的已识别键盘命令先 defer 当前 header 为 keyboard focus，再处理激活或导航；repeat/keyup 保持消费且不重复激活。测试从真实 pointer click 开始分别发送 Space/Enter，检查 modality、focus-visible 及实际 outline 绘制；重复 keydown 和 keyup 不增加切换，单独消费 keyup 仍更新 keyboard presentation。

实际检查结果：

- Debug affected suite **13/13 通过**，3.91 秒。包含 Collapse public/theme/component、Focus order/state/lifecycle、Modal boundary/component、feedback component、Tabs/Select/Tooltip component、backend boundaries。
- Release expanded suite **14/14 通过**，3.00 秒；在上述 13 项之外包含 standalone `portable.button_component`。
- 扩展的 standalone Button **Debug 基线失败**：最初 14 项运行中其余 13 项通过，该项退出 `0xC00000FD`。在本工作区临时恢复 `815a509` 的原始 Collapse 源码，重建并运行未修改的 Button test，得到相同退出码；随后恢复 R2 源码并重新构建、运行上述 Debug 13 项。没有修改 Button 测试、提高其 stack 或把该项记为通过；这不是本轮 R2 新增失败，保留给父任务处理。
- clang-format 22.1.3：`format-code.py --check` **537 文件、0 failures**；doctor 健康、strict **67/67**、`git diff --check` 通过。
- 本轮不改 Select/Tooltip cleanup、Theme/goldens、renderer/scene ABI、根工作区、Gallery 或全局文档；不 push、PR、archive 或创建子任务。仍不声称 GPU/Linux/人工 IME 验收。

重现本轮检查使用以下 targets/filter，Debug 运行 13 项；Release 将 `rynui_portable_button_component` 加入 targets、`button_component|` 加入 portable filter 后运行 14 项。构建及 CTest 仍使用上方的正式 preset 命令与 `--parallel 4`。

```powershell
$targets = @('rynui_collapse_component_tests', 'rynui_collapse_public_api_tests', 'rynui_collapse_theme_tests',
  'rynui_portable_focus_order', 'rynui_portable_focus_state', 'rynui_portable_focus_lifecycle',
  'rynui_portable_modal_boundary', 'rynui_portable_modal_component', 'rynui_feedback_component_tests',
  'rynui_portable_tabs_component', 'rynui_portable_select_component', 'rynui_portable_tooltip_component')
$filter = 'rynui\.(collapse_.*|feedback_component|portable\.(focus_(order|state|lifecycle)|modal_(boundary|component)|tabs_component|select_component|tooltip_component|backend_boundaries))$'
```
