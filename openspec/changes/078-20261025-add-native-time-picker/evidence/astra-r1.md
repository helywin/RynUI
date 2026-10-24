# Astra R1 五项 P2 修复证据

2026-10-25，在本分支已包含父基线 `0fdefbae9d5fb67422d6a67471019d5e3227dac1` 的 `777f905f2d91e6f47dcf64c2a64805efb5b645d2` 上复现。Astra R1 不是 clean；原有 scoped combined 35/35 通过没有覆盖下列问题。本记录补充先失败后修复的实际执行证据，不把原测试通过解释为无缺陷。

## 修复前复现

仅新增 `time_picker_review_tests.cpp` 与 CMake 注册、尚未修改 production code 时，Windows / MSVC / `windows-msvc-headless` / Debug 执行：

```text
ctest --preset windows-msvc-headless-debug -R '^rynui.time_picker_review_' --output-on-failure
```

结果 **0/5 通过，5 项失败，0.74 s**：

| CTest 后缀 | 实际首个失败消息 |
| --- | --- |
| open | `R1: ordinary controlled close request overrode parent authority` |
| activation | `R2: legal unchanged zero row did not activate immediate empty selection` |
| reset | `R3: same accepted Form reset retained TimePicker model/input draft` |
| cycles | `R4: stale focus tracking closed programmatic new open cycle` |
| focus | `R5: footer Input lost exactly-once onBlur during keyboard close` |

## 修复与断言

1. 普通关闭不再设置 suppression，受控父级可接受或拒绝；强制 Modal/Spin、祖先隐藏、disabled、窗口失活走独立 suppression。实际 pointer/keyboard 覆盖 Escape、OK、Cancel、outside 各自接受/拒绝/重试；待确认 09:00:00 只有 OK 提交，其余恢复 08:00:00。outside fixture 检查真实命中的是外部 Button。强制边界解除后不复活，显式打开才恢复。
2. 行的合法激活通过 `candidate` 判断，不用 `select` 的 changed 返回值作为合法性；合法点击始终设置 active column，并 deferred focus selector。空值即时模式点击 00 请求有效零点；受控拒绝可重试，已接受相同值不重复发值，已选分钟行后 Down 改分钟，禁用行不激活且导航跳过。
3. 保持内部 `InputComponentHost::configure_form_reset(ComponentId, std::function<void()>)` 与 `InputState::on_owner_form_reset` 形状；取消 preedit 后复制/调用 owner callback，在既有 number/string reconcile 前重取同一 component generation、active scope 和 editor id。Time owner 从当前受控 Prop 恢复 model，或取消未提交状态，不发值回调。实际 String Form adapter 的同值 reset 覆盖完整输入、未完成输入、preedit、面板即时请求被拒绝；普通 validation/help/Theme 更新保持草稿，reset 后两次 Enter 不复活 09:30:00，Input/rows/footer 身份不变。旧 ref reset 不跨新 generation，dispose 后安全。
4. 实际开合变化重置 focus tracking；已聚焦周期关闭后，连续三次不聚焦的程序化 false→true 均保持打开。原 defaultOpen 与强制边界用例一并保留。
5. 隐藏 popup 仅取消 pointer interaction，保留现有 FocusManager sanitize/deferred blur 路径；不修改 FocusManager 通用语义。NORMAL motion 下实际 footer Input 的 `onBlur` 恰好一次，内部 focused、FocusManager 和 text session 一致，selector 恢复；随后 blur 结束 caret deadline。有限焦点过渡结束后 tick 无新动画、无 pending frame/deadline。另测 onBlur 同步销毁并复用 ref 创建新 generation，以及同步受控重新打开，不泄漏旧焦点/session/caret。

## 本阶段实际验证

官方 MSVC / Ninja Multi-Config preset，build 最大 `--parallel 4`，Debug/Release 均完成相应六个 TimePicker targets 构建。最终 focused 正则：

```text
^rynui.time_picker_(review_.*|model|public_api|component|theme|integration)$
```

- Debug：**10/10，8.29 s**。
- Release：**10/10，2.60 s**。
- clang-format 22.1.3：**623 文件，0 failures**。
- `openspec doctor --json`：healthy；`openspec validate --all --strict --no-interactive`：**79/79**。

本阶段保持 TimePicker 公开 API、FormValue 值域、Theme/IIFE/identity、host registration 与 popup 边界顺序。`InputComponentHost::focused` 仅内部诊断，用于实际 footer Input 焦点断言。共享 reset hook 保持与父任务约定的 API；父任务将独立落地 DatePicker 同值 reset，待干净 SHA 后合入，不从父任务脏树复制。

原 tasks 2–4 暂不恢复完成，等待最终受影响 scoped combined 和双基线 fresh goldens。尚未运行 fresh Astra R2、原生 GPU、Gallery、Linux 或系统 IME；这些不算本记录的通过项。

## Authority 读取期间销毁 owner 的补充修复

父任务指出 Time reset callback 在 `read_prop` 调用用户 Binding 前持有 `TimePickerState*`。先在真实 String Form adapter 加入 authority 读取时同步 `destroy(owner)` 的用例、保持 production code 为 `d4c3307`：Debug `rynui.time_picker_review_reset` **0/1，1.00 s**，实际报 `TimePicker precision is invalid`，证明 owner 已销毁后仍使用旧 model。

修复先将 authority 读入局部值，随后重新查找同一 component generation，并检查 active scope 和所捕获 editor id，才 reconcile/refresh。回归同时断言旧 Time owner、InputRef、session 均失效，随后调用捕获的旧 reset 安全返回；原新 generation 草稿不被旧 reset 修改的用例继续保留。共享 Input hook API/字段/实现没有在此阶段改变。

加入此用例后的完整 Time focused 结果：Debug **10/10，8.83 s**；Release **10/10，2.59 s**。clang-format 22.1.3 仍为 **623 文件，0 failures**。
