# Astra R2：footer 编辑后点击时间行

2026-10-25，基线 `b96c575350878b2f6ba23656e4b9aadd335b1280`。独立 Astra R2 指出一项 P2：`choose()` 将焦点从 footer Input 交回 selector 时，`defer_focus()` 可同步执行 footer `onBlur`；应用正常卸载面板后，原函数仍使用旧 owner state。本轮保留 R1 五项修复及 Date/Form shared reset，不合入新 main 或 Gallery WIP。

## 修复前实际复现

先只新增 review case 和 CMake 注册，production code 保持基线。用既有 Input fixture 创建 `defaultOpen`、`needConfirm(false)` 的 TimePicker 与带 `onBlur` 的 footer Input；真实编辑 footer，确认实际命中可见小时行，再派发 mouse down/up。`onBlur` 卸载 TimePicker，窗口服务本身继续存活。

```text
ctest --preset windows-msvc-headless-debug -R '^rynui.time_picker_review_row_focus$' --output-on-failure
```

有效复现结果：**0/1，0.17 s，Exception: SegFault**，最后输出为 `row blur destroy confirm=0 rebind=0`。没有用键盘 Escape 或直接调用私有 `choose()` 代替该 pointer 流程。

搭建 fixture 时先纠正了屏外行的命中位置；有效复现使用可见行并断言真实 hit owner。窗口状态子用例采用既有 host 通知，避免在 footer callback 中重入全局 FocusManager dispatch；没有修改该通用调度合同。

## 最小修正与定向审计

`choose()` 在 `defer_focus()` 返回后，按原 ComponentId 重新查找同一 generation。只有 active scope、visible、未 disabled、窗口 active、当前输入边界允许，并且 selector 仍存活、eligible 且 active 时，才读取新的 `needConfirm` 与 model 并决定即时提交。不保留回调前的 pending 值，不重新应用旧行覆盖回调后的 authority。

production 仅新增 **11 行**；没有公共 API、共享 Input、Date/Form、Theme、FocusManager 或 Gallery 改动。

邻近审计：

- `request_open()` 已在 onOpenChange 后重新查找 owner，其 `defer_focus()` 为末尾操作，没有随后读取旧 state。
- row pointer handler 在 `choose()` 返回后没有继续访问原 row/owner。
- `publish()` 复制 onChange，并在回调后检查 owner 仍存在才请求关闭；`confirm()` 没有在 publish 返回后继续读取旧 state。
- `refresh()` 仅取消 pointer interaction，保持 R1 的正常 deferred focus/blur 交付；没有新增即时焦点回调边界。

## 新用例的有限覆盖

`row_focus` 共覆盖 13 个具体场景：

- 即时/确认各自卸载，或在同一 onBlur 中卸载并用同一 ref 创建新 generation，共四种。检查旧值回调为零、旧 row hit 消失、旧 focus/session/caret 结束；旧 text session 不影响新组件，新的 03:00:00 draft/pending 不被旧事件改变，随后新行点击仍可提交 04:00:00。
- 存活 owner 的显式关闭、disabled、Spin、Modal、窗口 host inactive、祖先隐藏，各不发布失效请求。
- footer callback 更新受控值、rules、needConfirm：使用新的 model/规则；切到确认模式后不即时发布，随后 Enter 请求合法新值，受控父级拒绝时仍保留 authority。

修正后的单独新用例 Debug **1/1，2.81 s**。既有合法相同行激活、受控拒绝/重试、同值 reset/preedit/authority 读取卸载、新开合周期、NORMAL motion 的 blur/焦点/caret 及回调重建回归全部保留。

## 最终实际验证

Windows、MSVC、`windows-msvc-headless`、Ninja Multi-Config，build 最大 `--parallel 4`。构建 review、Time model/public API/component/Theme/integration 和 Date review targets；另构建 Debug Theme algorithm golden generator。

在对应 Debug/Release test preset 带 `--output-on-failure`，精确 `-R`：

```text
^rynui\.(time_picker_(review_.*|model|public_api|component|theme|integration)|date_picker_review_form_reset)$
```

- **Debug 12/12，12.57 s**。
- **Release 12/12，3.95 s**；新 row_focus 用例 **1.00 s**。

包含新用例、原五个 review、Time 五个 model/API/component/Theme/integration 和 Date form_reset。仅有 Time host 改动，没有重跑无关完整矩阵或原 combined 35；不把基线或父线程的旧运行写为本轮重跑。

用本树新链接的 Debug Theme algorithm executable 重新生成五份 goldens，与 `b96c575` 包含 identity 的文件字节全部相同。解析比较 `78c4adf` 的 37 个旧字段和 `0fdefbae` 的 38 个旧字段（排除 identity），全部嵌套值相等，Date/Form 旧字段保持；SHA-256 与 [parent-integration.md](parent-integration.md) 的五项记录一致。

源比较确认 shared Input `.cpp/.hpp` 仍只比 `0e0f11f7` 多唯一 `focused()` 诊断；owner reset 字段/声明/定义各恰好一个。Input、Date 实现与 review、公开 headers、Theme、Gallery 和完成目录相对 `b96c575` 没有变化。

clang-format 22.1.3：**623 文件，0 failures**；doctor healthy；strict validation **80/80**，diff check 通过。本轮不声称 fresh Astra R3 已通过；由父线程安排。原生 GPU/Gallery 窗口、Linux 和系统 IME 不属于本轮已运行证据。只创建本地英文 Conventional Commit，没有 push、PR、archive、子任务或父工作树编辑。
