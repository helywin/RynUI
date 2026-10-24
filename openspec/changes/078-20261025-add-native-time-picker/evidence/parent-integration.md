# DatePicker / Form 父基线合并验收

2026-10-25，按用户指令在 TimePicker 自有阶段已提交且工作树干净的 `78c4adf196ce268973fdafa4a43ace8cd1e6640b` 上，执行 `git merge --no-ff --no-commit 0fdefbae9d5fb67422d6a67471019d5e3227dac1`，验证后创建本地 merge commit。没有 rebase、push、PR、archive 或子任务。

本记录是合并后的证据；此前 `windows-msvc-headless.md` 记录的是独立 TimePicker 阶段，不代表合并后的 Theme 配置布局或测试总数。

## 保留边界

- 父版 Date/Form 的公开类型、实现和 tests 文件与 `0fdefbae` 保持一致；TimePicker 的公开 headers、tokens 与 model 与 `78c4adf` 保持一致。079 规划、Gallery、README 和全局完成目录只随父基线进入，没有本阶段修改。
- 逐段比较确认父版 **26 个 sequential IIFE byte-identical**；原 `TokenIdentity` 顺序完整保留，TimePicker 四项追加在后。保留 Date/Form/Time 三套 Theme 配置、序列化、继承与所有宿主注册。
- 父版 `ThemeScope::create_default()`（含 `static const ThemeConfig`）逐字保持；generic heap descendant preflight、commit helper、binding bridge 和 self-config guard 不改。`theme_runtime.cpp` 相对父版只新增 TimePicker 追踪项、accessor 与 dirty 分组。
- 宿主原有相对顺序保留；全部内容 scroll handlers 运行后才按 TimePicker → DatePicker → Dropdown 进行边界兜底，各自检查实际 hit 的最近 popup owner。

## 复现与修复

1. **混合 footer 首帧 anchor**：`TimePicker(defaultOpen)` footer 中放 `DatePicker(defaultOpen)`，首次同步时 Date 宿主先于 Time 运行，Date 的 bounds 仍引用未完成父层定位的旧 anchor；新断言报 `mixed footer popup did not use the final parent anchor on its first frame`。修复仅在实际 Time 定位改变且 footer 有后代 Date 时，复用 Date 原有 position pass，随后才放置更深的 Time 子层，并在最后保留既有 Table clip/translation 同步。无循环重试或空闲 pass。
2. **子 Date Escape ownership**：Date 输入位于父 Time 的 popup layer，反向 keyboard handler 顺序让 Time 抢先取消父层；新增真实 focus/Escape 用例报 `child Date Escape closed parent Time`。Time 现在沿组件祖先检查更近且已打开的 Date owner，把该输入交还 Date；两个嵌套方向均验证子 Escape 只关闭子层。
3. **公开无参 Theme 调用的调用方栈**：先移除 Time 分支旧的 default-config helper、仅保留父版根创建修复后，现有 Button Debug 再次退出 `0xC00000FD`。CDB 显示根创建 frame 已很小，但 `test_reactive_state_matrix_and_minimal_dirty_ranges` 调用方约 853 KiB，继而在 `resolve_theme` 首个 component IIFE 的 `__chkstk` 耗尽栈。根函数保持父版原样；公开 `resolve_theme()` 的默认参数另引用 `detail::default_theme_config()` 的不可变 `static const ThemeConfig`，避免每个调用表达式保留全量默认参数。原函数签名及 IIFE 不变，未增加栈限制。Button Debug/Release 随后均通过。

## 组合用例与实际结果

Windows、MSVC、`windows-msvc-headless`、Ninja Multi-Config，build 最多 `--parallel 4`。受影响 scoped combined CTest：

- **Debug 35/35，35.84 s**。
- **Release 35/35，17.37 s**。

在对应 test preset 上带 `--output-on-failure`，使用下列精确 `-R`：

```text
^(rynui\.(time_picker_(model|public_api|component|theme|integration)|date_picker_(model|public_api|component|theme|review_(wheel|metrics|inheritance|preflight))|form_(model|public_api|component|theme)|table_component|table_theme|feedback_component)|rynui\.portable\.(input_component|input_variant|input_actions|input_public_api|input_caret_blink|button_component|theme_algorithm|theme_runtime|select_component|select_theme|popconfirm_component|dropdown_component|modal_boundary|modal_component|backend_boundaries))$
```

新增 `time_picker_integration` 组合覆盖：Time footer Date、Date footer Time 的首帧定位/滚轮/Escape，Time → Date → Dropdown 边界链，以及真实 Form 的 required/status、两个 picker 输入提交、submit、disabled、reset 和 retained Input identity。Form075 公开值域保持原样，测试用显式 String field ↔ typed picker adapter，不新增 Date/Time FormValue kind。原有 footer Table/Select、嵌套/兄弟 popup、Modal/Spin/隐藏/失效 ref/回调销毁用例继续通过。

没有运行无关完整矩阵。原生 GPU、Gallery、Linux、系统 IME，以及父任务此前的 native inventory/smoke，均不作为本阶段已运行证据。

## 双基线 fresh goldens

用本树 Debug `rynui_portable_theme_algorithm.exe --write-goldens` 生成五份，再解析 JSON 与两份 Git 基线逐字段比较：

- 相对 `78c4adf`：**37 个既有顶层字段及全部嵌套值相等**（仅排除 identity），新增集合恰为 `{datePicker, form}`。
- 相对 `0fdefbae`：**38 个既有顶层字段及全部嵌套值相等**（仅排除 identity），新增集合恰为 `{timePicker}`；Date/Form 字段完整保留。

| 文件 | 合并后 SHA-256 |
| --- | --- |
| default.json | `e5dd40343c78723473067394117e6a9edccdf7ce46be77b80735d1b3088ce294` |
| dark.json | `4da9543fbed00da35467b2eb03e5b73754486bf304e7a0b5d4a4a6efc4d88050` |
| compact.json | `9d9877b575254d5cfaa2cb6af7000fbdaf2d15760e081b4be9e4e229e8f69f2d` |
| dark-compact.json | `b04b759e17d24d2ec9784ecadc3e171e312cd18b4583bc24ae172a0d07d97918` |
| compact-dark.json | `962956350310060672c550e6a197921313f4a5bc513c397a06f4dfd257585736` |

## 检查

clang-format **22.1.3** 的 `scripts/format-code.py --check`：**622 文件，0 failures**。doctor healthy，strict validation **79/79**；Core/renderer configure/link 守卫、Debug/Release `portable.backend_boundaries` 与 diff check 通过。此处无新 Astra review 声明，交由父任务安排。
