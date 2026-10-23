# Collapse 平台通用验收

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
