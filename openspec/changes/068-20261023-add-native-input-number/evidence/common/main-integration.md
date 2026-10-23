# main 共同集成验收（2026-10-23）

## 输入与环境

- 工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-de868eb7`；本地分支：`t3/integrate-menu-input-number`。
- 初始 main：`69689046bc9a99b94c98086cb3c849b79015acbc`。
- Menu/Dropdown：`4fa26a9787c25a706ac1df39f687d856dd0c3851`，本地合并提交 `37c6dda`。
- InputNumber：`8cea30f3738c8f79e4ace10702c7c11b5888a991`，本地合并提交 `fa0bb4e`（包含 Spin 直接回归）。
- 父任务随后授权合并 Table/Pagination main：`b27651db3b8e2b6ca1edae01b722390819385e24`。本记录验证上述合并结果及下述 Theme binding 栈占用修复。
- Windows x64，MSVC 19.51.36256，Ninja Multi-Config，`windows-msvc-headless`；全部二进制由本工作树独立构建，单个构建最多 4 jobs。

## 最终结果

- Debug：聚焦 CTest **34/34**，14.59 秒。
- Release：相同聚焦 CTest **34/34**，10.53 秒。
- 包含四个 InputNumber、Menu/Dropdown、四个非 portable 的 Table/Pagination、Input/editor/session、Theme、Modal/Drawer/Popconfirm、Tag/Badge、Alert/Spin/Progress、Switch features 和 backend boundaries。
- clang-format 22.1.3 检查 **560** 个自有源文件，0 failures；doctor healthy；strict validate **69/69**；diff check 通过。
- 没有重复完整 117/119 套件或 Astra 评审；没有运行原生/GPU/Gallery、系统 IME、Linux 或人工视觉验收。

实际 CTest filter：

```text
^rynui\.(pagination_component|table_component|table_theme|table_public_api|input_number_model|input_number_public_api|input_number_component|input_number_theme|feedback_public_api|feedback_theme|feedback_component|tag_badge_public_api|tag_badge_theme|tag_badge_component|portable\.(switch_features|menu_component|dropdown_component|theme_algorithm|theme_runtime|input_component|input_variant|input_actions|input_public_api|input_caret_blink|password_component|search_component|text_area|text_input_session|text_editor|modal_boundary|modal_component|drawer_component|popconfirm_component|backend_boundaries))$
```

对应 executable targets 为 filter 内非 portable 名称的 `rynui_<name>_tests` 与 portable 名称的 `rynui_portable_<name>`；`backend_boundaries` 为脚本测试，无 executable target。Debug / Release 分别运行：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target <上述 executable targets>
ctest --preset windows-msvc-headless-debug --parallel 4 -R "<上述 filter>" --no-tests=error
cmake --build --preset windows-msvc-headless-release --parallel 4 --target <上述 executable targets>
ctest --preset windows-msvc-headless-release --parallel 4 -R "<上述 filter>" --no-tests=error
```

## 新增跨组件回归

`tests/input_number_component_tests.cpp::spin_preserves_numeric_input_gates` 使用实际 Spin loading Signal、PointerRouter、FocusManager、InputRef 和 text session，覆盖 editable / disabled / readOnly 三种恢复路径。loading 期间取消 preedit/session，阻止真实按钮、Up/Down、ref focus/select 与迟到 IME；结束 loading 后保留自身 disabled/readOnly。重新进入 preedit 仍禁止步进，取消后按钮及键盘恢复；值、editor/action/text identity 和单次 slot mount 保留。最终两个 configuration 均通过。

## 实际发现并修复的集成问题

第一次 Table-inclusive Debug 为 **33/34**，新增的 Switch features 失败。直接运行退出 `0xC00000FD`；CDB 确认为 stack overflow，调用链为 `Signal<ThemeConfig>::set → connect_binding → connect_prop → ThemeScope::update → resolve_theme`。main 的 `commit_snapshot(const ThemeConfig&, ThemeSnapshot&&)` 修复没有丢失；合并全部 tokens 后，generic property callback 链仍多次按值传递大型 ThemeConfig。

仅改 const-reference callback 仍在 derive_button 的栈探测失败。最终仅修改 `src/component/theme_component.cpp`：先完成 Binding 求值并以 immutable shared storage 保存配置，再进入 snapshot resolution；缓存同一配置以保持重复值抑制，继续使用 binding phase、weak scope 和 lifetime cleanup。未修改 generic Prop/Binding API、线程栈大小或 Switch 测试。独立 Switch Debug 先恢复为 1/1，再重新链接全部受影响 targets，最终 Debug/Release 34/34。

## Golden 与共同合同

五份 golden 均通过新构建的 merged Debug `rynui_portable_theme_algorithm --write-goldens` 重新生成；Release Theme algorithm 使用相同 golden 通过。

- 与 `6968904` 比较：除 `identity` 外全部 27 个旧顶层字段的完整嵌套值不变；仅增加 `menu`、`dropdown`、`inputNumber`、`pagination`、`table`。
- 与 `b27651d` 比较：除 `identity` 外全部 29 个旧字段不变；仅增加 `menu`、`dropdown`、`inputNumber`。五份文件共完成十次 JSON 语义比较。
- 保留 Modal/Drawer/Popconfirm/TagBadge/Alert/Spin/Progress 和 Table/Pagination 的 Theme、service、CMake、blocked subtree 输入与参与者 lifetime hooks。
- 前向 geometry 的关键顺序为 `Modal → Table → Popconfirm → Dropdown → Tooltip → OTP → Select`，Menu host 在其后处理自身行；keyboard/scroll 保持反向分发，Menu wheel 仍使用 topmost hit ownership。
- 保留 main 的 popup child-layer 排序、ThemeScope 引用提交与 reference_cell 默认主题栈占用修正。`README.md`、`README.en.md`、`docs/component-completion.md` 自动合并后与 `b27651d` 完全一致；没有自行编辑全局 Gallery、support overlay 或这些文档。

本地原始记录位于 ignored `out/integration-final-{debug,release}-{build,relink,ctest}.log`、`out/integration-final-golden-comparison.log` 和 `out/integration-switch-stack*.log`。只创建本地英文提交；没有 push、PR、archive、子任务或远端 t3 分支操作。原生 Gallery 和最终 main 推送仍由父任务执行。
