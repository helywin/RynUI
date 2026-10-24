# 078 平台通用实现与验收记录

日期：2026-10-25。规划基线：`0bda6f0865108f1d1d44504b8dc4ad5245a068af`；实现提交：`71ce319`、`a0ce57f`。本工作树独立实施，未合入 077 DatePicker 分支，未修改 Gallery、README 或全局组件完成目录。

## 环境与执行范围

- Windows，MSVC 19.51.36256，`windows-msvc-headless`，`Ninja Multi-Config`，BUNDLED 锁定依赖，RECORDING renderer；每次 build 最多 `--parallel 4`。
- 使用 Visual Studio x64 Developer Environment 后，`cmake --preset windows-msvc-headless` configure 与真实 headless 构建成功；Core / renderer include/link configure 守卫通过，`rynui.portable.backend_boundaries` 通过。
- 这是一组平台通用 CPU/保留组件合同，未运行 Gallery、原生窗口/GPU、Linux、系统字体或真实系统 IME。preedit 证据来自真实 Input 编辑器的 headless 事件端口，不代表系统 IME 验收。新的 Astra review 由父任务安排，本记录不声称已通过该 review。

## 实际测试结果

第一阶段 model/public API：Debug、Release 各 **2/2**。完整受影响范围 Debug **23/23**（33.79 s）；随后只修改 TimePicker 的 panel-only Theme 唤醒并重建，最终 TimePicker 五项 Debug **5/5**（5.23 s）。最终受影响范围 Release **23/23**（15.62 s）。没有扩跑无关完整 CTest 矩阵。

完整范围使用的 CTest 正则如下，分别对应 `ctest --preset windows-msvc-headless-debug` / `ctest --preset windows-msvc-headless-release`，均带 `--output-on-failure`：

```text
^(rynui\.time_picker_(model|public_api|component|theme|integration)|rynui\.(table_component|table_theme|feedback_component)|rynui\.portable\.(input_component|input_variant|input_actions|input_public_api|input_caret_blink|button_component|theme_algorithm|theme_runtime|select_component|select_theme|popconfirm_component|dropdown_component|modal_boundary|modal_component|backend_boundaries))$
```

最后一次 Debug 局部回归使用：

```text
ctest --preset windows-msvc-headless-debug -R "^rynui\.time_picker_(model|public_api|component|theme|integration)$" --output-on-failure
```

构建使用各自 build preset（`windows-msvc-headless-debug` / `windows-msvc-headless-release`）及以下 22 个实际 executable target；boundary 为脚本测试：

```text
rynui_time_picker_model_tests
rynui_time_picker_public_api_tests
rynui_time_picker_component_tests
rynui_time_picker_theme_tests
rynui_time_picker_integration_tests
rynui_feedback_component_tests
rynui_table_component_tests
rynui_table_theme_tests
rynui_portable_dropdown_component
rynui_portable_input_component
rynui_portable_input_variant
rynui_portable_input_actions
rynui_portable_input_caret_blink
rynui_portable_input_public_api
rynui_portable_button_component
rynui_portable_theme_algorithm
rynui_portable_theme_runtime
rynui_portable_select_theme
rynui_portable_select_component
rynui_portable_modal_boundary
rynui_portable_modal_component
rynui_portable_popconfirm_component
```

## 覆盖的真实合同

- `time_picker_model`：00:00:00 / 23:59:59、严格 ASCII 两种格式、非法/未完成文本、有效值构造、范围/步长、前缀合法完成、全禁用有限导航、clear 的双层 optional、取消、受控拒绝与分钟模式保留外部秒/新提交归零秒。
- `time_picker_public_api`：typed Props、`Prop`/Signal、footer 类型隔离、安全 ref；编译并链接 `docs/time-picker.md` 中的实际示例与真实 `TimePicker` 实现。
- `time_picker_component`：真实 Input 与保留 24/60/60 行、pointer 选择、方向键/Home/End/列切换/Enter、默认确认与 Cancel/Escape、即时请求不关闭、wheel 只滚动、输入提交/失焦恢复、preedit、clear、受控拒绝、三尺寸/四变体/状态转发、Input/行/footer 身份、失效 ref、全禁用和空闲请求。
- clip 断言读取实际 glyph instance 的 position/translation/clip 后的可见覆盖、实际 ordered-scene selected surfaces，以及真实 hit-test；窄宽度、短视口及恢复后均检查分配列/行范围。未聚焦的打开面板和已关闭面板稳定同步后没有持续帧或动画 deadline；聚焦 Input 的 caret 使用现有 Input 机制。
- `time_picker_theme`：defaults、Dark/Compact、局部算法、partial inheritance、inherit=false、关闭算法时忽略局部 seed、hash/equality/diagnostic JSON、colors/metrics/effects/order dirty 分组；非法普通/最终派生尺寸、非有限值、不可渲染字号与后代继承失败在发布前拒绝。组件测试另验证只变 panel 背景或 z-index 也唤醒帧。
- `time_picker_integration`：真实 footer Input/Select/Table、空 Table body 横向滚动、内容先于边界兜底、最近子 popup wheel ownership、父子 TimePicker 定位/滚动/Escape、重叠兄弟 Select、初始受控打开、Spin/Modal/隐藏/窗口失活后抑制旧弹层、Modal 内只读输入使用列、disabled、onChange/onOpenChange 回调销毁、dispose 与失效 ref。

## 复现后修复的问题

1. 首次 popup 定位之前的 Table pass 缓存了空 ancestor clip：复现为 footer Table body `0×0`，最大纵向滚动 578。TimePicker 完成保留 footer 定位后复用 Table 原有几何同步；实际 body 恢复 `300×70`，子 Table 与 Select 各自滚动，父级/底层不滚动。不改变宿主注册、Modal → Table → popup 的原有 pass 或内容先行的事件分发顺序，不修改 Table 实现。
2. 新增列分隔线时固定 surface 层数不匹配：改为始终四层（背景、两个列分隔、footer 分隔），分钟模式保留空的第二分隔层；没有可变上传或组件私有 renderer 路径。
3. 现有 Button Debug 回归出现 `0xC00000FD`。CDB 调用栈指向约 842 KiB 的测试调用方 frame，随后经 `ThemeScope::create_default → resolve_theme → ButtonThemeToken` 耗尽默认栈；多次 `resolve_theme()` 的默认参数在调用方保留大型 `ThemeConfig` 临时量。改为 `detail::default_theme_config()` 返回共享堆上不可变空配置，函数签名和原 26 个 sequential IIFE、snapshot heap/commit helper、reactive bridge 与继承 preflight 保留；未提高线程栈。Button Debug/Release 回归随后均通过。

## Fresh goldens 语义比较

由本树 Debug 的 `rynui_portable_theme_algorithm.exe --write-goldens` 实际生成五份。对每份解析 JSON 后，以 `git show 0bda6f0865108f1d1d44504b8dc4ad5245a068af:<path>` 为基线，验证新顶层字段集合恰为 `{timePicker}`；删除新字段和双方 `identity` 后，**原有 36 个顶层字段及全部嵌套值完整相等**。不是只检查生成成功。最终 hashes：

| 文件 | SHA-256 |
| --- | --- |
| default.json | `0e0d208639d31b51062ab44d11729fab4cbc5a3bb5644b2d00edc52c949bee79` |
| dark.json | `5fbb4b058dcea5eb13888e4e6cff16f54f996779c6fd8b4d58c11974208b6faf` |
| compact.json | `36f59b5b7c41e3f2b9e4d07b93fd1379ac25f3ff2fc33b7896549578fb863379` |
| dark-compact.json | `ef4d4b8289379979af32807863b1711bec965e377634706dfe5534a99fb079a2` |
| compact-dark.json | `309664e90180103afa293736c9d229bea054015fbba5a929020dad77fa0982b7` |

## 代码与文档检查

`python scripts/format-code.py --check` 使用 clang-format **22.1.3**，检查 **596** 个自有文件、0 failures。`openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` **77/77**；`git diff --check` 通过。所有提交仅保存在本地，未 push、PR 或 archive。
