# Astra R1 两项 P2 修复与实测

## 基点和范围

2026-10-25，从干净的 `2205c1b10bed242a14b6ff4c64540699b8162e4b` 开始，分支 `t3/native-message-notification`，worktree 为 `C:/Users/jiang/.t3/worktrees/RynUI/t3-f0b94dd3`。复用 085 apply workflow，重开 2.1、2.3、2.4、3.1；没有创建新 change 或新功能。

生产代码只修改 `notice_component.cpp` 和 `retained_surface_service.cpp`。未合入 Popover、087、main/Gallery；未操作 ROOT、Git 远端、push、PR、archive 或 children。Theme、goldens、renderer、字体、clock 和 close/expiry 批次逻辑均未改。

## 先新增失败复现

先只添加 `notice_review_tests.cpp` 与 CMake 注册，保持生产代码为 2205c1b1；使用本树正式 MSVC Ninja Multi-Config headless presets、最多 4 jobs 构建新测试程序。

1. `notice_review_fills`：普通 720×200 窗口，Notification description 放四个 height=40、flex_shrink=0 的实色 Button。确认四个 child 高度仍为 40、区域外不能命中 child，随后直接遍历实际 ordered scene draws 并采样 quad/rounded-effect 覆盖率。Debug 和 Release 均失败：`overflow ordered fill coverage=1`，提示通知区域外发布了未裁剪填充。
2. `notice_review_trim`：maxCount=3，persistent A/B/C，A.onClose 关闭 C，然后上限改为 1。Debug 和 Release 均失败：`trim remaining=0, close callbacks=3`；B 被旧删除数量额外淘汰。
3. 同一缺陷路径的补充 Debug 失败复现：`notice_review_content_ranges` 使用真实 Slider dots，区域外 ordered dot coverage 为 1；`notice_review_trim_callbacks` 中 A 回调将上限重新提高到 3，原实现只保留 1 条，证明只读取尚未提交的缓存上限仍不够。

主 repro 修复前的结果是 Debug **0/2 passed**、Release **0/2 passed**；补充两个 Debug repro **0/2 passed**。这些失败均在修改生产代码前实测。

## 最小修复

- P2-1：共同 retained fill 裁剪的 surface 与 content-range 两个准入条件由 `in_window_layer_stack` 改为 `in_window_layer`，接纳普通 notice layer 后代。沿用原有 ancestor content/decoration clip、fill effects、shadow/focus 顺序和 window-layer root clip boundary，不更改实际 child 尺寸、GPU ABI 或 normal-flow 准入规则。
- P2-2：holder 保留 maxCount Prop source；trim 每轮读取当前 authority，并在 getter 后按完整 holder generation/存活 scope 回取状态。每次关闭回调后重新判断 live count 与最新上限，达到上限立即停止，不保存旧指针、旧 batch 或删除数量。直接读取 source 能覆盖同一 reactive observer 内尚未提交的上限变化。

## 修复后实际证据

两项 P2 修复的去重集合使用以下正则，分别运行 `ctest --preset windows-msvc-headless-debug` 与 `ctest --preset windows-msvc-headless-release`，均附 `--output-on-failure`。当时 review 注册包含 fills/content_ranges/clip_contract/trim/trim_callbacks 五项，尚未加入下面父追加核对的 anchor 用例：

```text
^rynui\.(notice_review_.*|notice_(retention|clock|lifecycle|boundaries|popup|clipping)|portable\.(modal_component|button_scene_service))$
```

- Debug：**13/13 passed**，6.41 秒。
- Release：**13/13 passed**，2.79 秒。
- 构建目标：`rynui_notice_review_tests`、`rynui_notice_component_tests`、`rynui_portable_modal_component`、`rynui_portable_button_scene_service`，及正式依赖；每次最多 4 jobs。没有运行历史全套或 24×2 集合。
- `notice_review_fills`：实际 ordered fill coverage 从 **1 → 0**；可见 Button 内 coverage 仍大于 .99；四个 child 保持 height=40；裁剪外 hit 不命中对应 Button。
- `notice_review_content_ranges`：真实 Slider dot 的区域外 coverage 从 **1 → 0**，区域内 dot coverage 仍大于 .9，覆盖 content-range 路径。
- `notice_review_clip_contract`：notice Button 不再发布源 quad fill draw；普通正文 Button 在 notice 显示与关闭后仍发布原始 quad。Button、borderless Input、Checkbox 的实际 focus effect 保留外扩覆盖率、正确外部 ancestor clip 和有效内容 hit，关闭后 refs 清理。
- `notice_review_trim`：从 **0 条/3 次回调 → 仅 B 一条/2 次回调（A、C）**，B 的完整身份保持。
- `notice_review_trim_callbacks`：提高上限从 **仅 1 条 → B/C 两条**；clear、同 key 重新创建、onClose 卸载/ref 重绑，以及 maxCount authority getter 同步卸载/ref 重绑全部通过。
- 原有 notice retention、clock、lifecycle、boundaries、popup、clipping 用例原断言全部保留并通过，涵盖 expiry 批次同 key generation、延长另一到期 entry、输入边界和子 popup wheel/Escape。现有 Modal 与 retained Button scene service 回归也通过。

## 父追加的同范围 anchor 核对

未确认额外生产缺陷，因此不调整 host 阶段或引入逐 popup 刷新。`window_component_services.cpp:109-118` 在基点已经按 Notice、Modal、Table、DatePicker、TimePicker、其余 anchored popup、Select 顺序构造并 attach；统一 position pass 顺序保持一致。`select_component.cpp:813-835` 在自己的 pass 读取已放置的 selector bounds，并与保存的 layout_anchor 比较重排 popup。

最小新增 `notice_review_anchor`：Modal/Spin 内的 Notification 同时在 description 与 actions 挂载已打开的 Select；首帧、reactive placement 变化、viewport 变化各只调用一次 `layout_and_synchronize`，检查两个 popup 相对新 selector 的水平位置/宽度与 above/below gap。保留组件身份，Spin suppression 与恢复再验证一次。

- Debug 单独 **1/1 passed**，0.25 秒；Release 单独 **1/1 passed**，0.16 秒。
- 实际 selector / popup：首帧 `(368,72)/(368,108)`、`(368,112)/(368,148)`；移动后 `(80,488)/(80,524)`、`(80,528)/(80,452)`；viewport 改为 900×500 后 `(80,388)/(80,424)`、`(80,428)/(80,352)`；Spin 恢复后同帧位置保持正确。
- 该追加只改测试/CMake，不修改生产代码；此前 13 项结果继续有效。本轮必要去重验收累计每种 configuration **14 项（13+1）全部通过**，未重跑历史 24×2。原 Modal/Spin/clip/foreign blocks 合同由上述组合及未改动的 notice_boundaries 断言共同覆盖。

## 收口检查与限制

- clang-format **22.1.3**，`scripts/format-code.py --check`：**689 files / 0 failures**。
- 使用 pnpm 已安装的显式 **OpenSpec 1.14.0** 包入口；doctor healthy，`validate --all --strict --no-interactive`：**84 passed / 0 failed**；`git diff --check` 通过。
- 这是 Windows MSVC headless 的逻辑场景、覆盖率和生命周期证据，不代替 GPU、native window、Linux、system IME 或 AT 验收。
- 本轮完成已分配 R1 问题后停止；独立 Astra R2 与主分支集成由父任务负责。
