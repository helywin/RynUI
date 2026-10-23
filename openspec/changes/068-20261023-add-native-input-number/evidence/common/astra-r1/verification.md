# Astra R1 两项 P2 复核

## 复现

基线为本地 `91f10194759cc499b9ef72da3883244ddfbd6293`。先仅增加回归测试，再对原生产代码运行 Debug 两个目标，**0/2 通过，2/2 按预期失败**，原始记录为 `reproduction-debug.log`：

- `input_number_model`：首个用例为 value=0.26、step=0.01、precision=1，Down 的方向不变量失败。旧流程把 0.25 最近舍入为 0.3；负数 Up 有镜像问题。
- `input_number_component`：composition 后 `ref.select(0,0)` 已取消 preedit，layout 后仍触发 `ref.select cancellation left actions disabled`。原代码中 focus cursor 与 pointer selection 也只更新文本，没有刷新数值资格；修复后分别由独立 fixture 覆盖三条路径。

没有丢弃或降级任一 finding。

## 修复

1. 数值步进保留原有最近舍入；如果舍入使候选跨过当前权威值而反向，则对未舍入候选沿请求方向量化。夹取到有效范围后再次比较非空权威值，方向相反则不变、不发 change/step。最小步长小于 precision 分辨率而原本无变化的行为保持；空值仍按 0 起步再夹取。
2. Input 文本同步比较编辑器实际 composition 状态与上一次数值资格快照，仅在状态变化时刷新；移除 Escape、composition 与同值 commit 分支的重复刷新。ref selection、focus cursor、pointer placement 与会话资格变化沿同一状态链路恢复按钮，不解析或发布 preedit，不每帧无条件刷新。

生产代码修改仅限 `src/component/input_number_model.cpp` 和 `src/component/input_component.cpp`。公开 API、Theme、CMake、scene v3/GPU ABI v2 均未改变。

## 回归覆盖

- 数值模型 15 组定向案例：正负号镜像、Up/Down、零边界、小数半值、precision=0、上下界向内量化、外部越界值以及合法无变化；外部 reconcile 和 precision 动态配置后保持方向。
- 组件覆盖原始 `defaultValue(0.26).step(0.01).precision(1)` 的 Down、controlled value/precision 更新后的键盘 Down 与按钮 Up，以及动态 min/max 下禁止反向但允许另一方向进入范围；检查实际 change 和 typed step direction。
- ref.select、focus cursor、pointer text selection 三条取消路径，各自检查 preedit 不发布、权威值/committed 文本不变、上下按钮恢复，再通过真实 PointerRouter 点击 Up、FocusManager 键盘 Down 完成两次步进。
- disabled、readOnly、共同 branch activity、Modal 使用的 input boundary 四类门禁，在组合态取消后仍拒绝步进；恢复门禁后按钮/键盘重新可用。逻辑门禁继续由 interaction registry 判断，没有改写真实 disabled/readOnly。
- 此基线没有 Spin 组件源码；没有从父分支引入或模拟声明 Spin 业务集成通过。已验证本基线现有共同逻辑门禁，父任务的 Spin 集成仍需其自身证据。

## 执行结果

独立构建目录仍是本工作树的 `out/build/windows-msvc-headless`，Windows x64 / MSVC 19.51.36256 / Ninja Multi-Config，最多 4 编译作业。

- Debug：受影响 CTest **12/12** 通过，原始 `debug-ctest.log`。
- Release：受影响 CTest **12/12** 通过，原始 `release-ctest.log`。
- clang-format 22.1.3：`scripts/format-code.py --check` 检查 **531** 个自有源文件，**0** 失败；`openspec doctor --json` healthy，strict validate **65/65**，`git diff --check` 通过。
- 当前 T3 selection 已只读核实为 `gpt-6.1-sol`、`reasoningEffort=xhigh`、`serviceTier=priority`，没有更改模型或创建子代理。
- 完整 CTest 未运行；本轮只覆盖四个 InputNumber 目标及 Input/component/actions/display、editor/history/reconcile、session、Modal boundary 合同。

在 MSVC Developer Environment 中执行（实际使用当前工作树 `out/input-number-dev.ps1` helper）：

```powershell
$r1Targets = @(
    'rynui_input_number_model_tests', 'rynui_input_number_public_api_tests',
    'rynui_input_number_component_tests', 'rynui_input_number_theme_tests',
    'rynui_portable_input_component', 'rynui_portable_input_actions',
    'rynui_portable_input_display', 'rynui_portable_text_input_session',
    'rynui_portable_text_editor', 'rynui_portable_text_reconcile',
    'rynui_portable_text_history', 'rynui_portable_modal_boundary'
)
$r1Filter = '^rynui.(input_number_(model|public_api|component|theme)|portable\.(input_component|input_actions|input_display|text_input_session|text_editor|text_reconcile|text_history|modal_boundary))$'
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target $r1Targets
ctest --preset windows-msvc-headless-debug --parallel 4 -R $r1Filter
cmake --build --preset windows-msvc-headless-release --parallel 4 --target $r1Targets
ctest --preset windows-msvc-headless-release --parallel 4 -R $r1Filter
```

系统 IME、真实 GPU 窗口、Linux 与人工视觉验收没有执行。仅本工作树本地提交；不 push、不建立 PR、不创建子代理，不改 root checkout/Gallery。
