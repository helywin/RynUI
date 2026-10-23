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

## Astra integration R1：Dropdown wheel 边界（2026-10-24）

在同一干净本地分支 `3b719286911b5496eb7fa0ae6d72050901d0013f` 上处理 P2；没有合并 root main 或触碰其他工作树。先仅增加真实 Dropdown-over-Table 测试，保持生产代码不变，四个独立 Debug case 均 exit 1，失败断言均为 `Dropdown boundary wheel scrolled covered Table`：

| case | 菜单 | 逻辑归属 |
| --- | --- | --- |
| `wheel-table-short` | 单项、无需滚动 | Table cell 内 |
| `wheel-table-boundary` | 长列表到达滚动下限 | Table cell 内 |
| `wheel-sibling-table-short` | 单项、无需滚动 | 与 Table 无祖先关系的 sibling |
| `wheel-sibling-table-boundary` | 长列表到达滚动下限 | 与 Table 无祖先关系的 sibling |

修复仅新增 Dropdown host 的 `on_scroll_input` fallback：Menu 和 child popup hosts 先处理 wheel；未消费的事件只有在 topmost hit 属于实际可见且可交互的 Dropdown popup 逻辑子树时才被该 popup 消费。这样菜单无法继续滚动或命中 popup padding 时，wheel 不再传给被遮挡的 Table。没有修改 Menu 的普通 inline ancestor handoff、Table 的滚动实现、Theme bridge 或 `Modal → Table → Popconfirm → Dropdown → Tooltip → Select` 关键 host 顺序。

四个组合场景同时检查：真实 topmost hit 与 popup/menu owner、长菜单先滚动且 Table 不变、边界不再滚动、popup padding 不滚动下面的 Table、露出 Table 区域仍能实际滚动、菜单和 Table row identity 保留、dispose 释放全部 interaction/surface。原有 child Select 与 nested Dropdown 在重叠区域和滚动边界的优先级，以及 inline Menu → Modal 的正常 ancestor scroll handoff 继续通过。

实际使用 MSVC 19.51.36256 / Ninja Multi-Config / `windows-msvc-headless`，Debug 与 Release 顺序构建、每次最多 4 compile jobs，targets 为 `rynui_portable_menu_component`、`rynui_portable_dropdown_component`、`rynui_table_component_tests`、`rynui_portable_select_component`、`rynui_portable_modal_component`、`rynui_portable_modal_boundary`。两种 configuration 使用相同 CTest filter：

```text
^rynui\.(table_component|portable\.(menu_component|dropdown_component|select_component|modal_component|modal_boundary))$
```

- Debug：**6/6**，2.96 秒；Release：**6/6**，1.37 秒。Dropdown executable 不带参数运行全部 case，包含上述四个新增 case。
- clang-format 22.1.3 检查 **560** 个自有源文件，0 failures；doctor healthy；strict validate **69/69**；diff check 通过。
- 原始本地记录：ignored `out/integration-r1-reproductions.json`、`out/integration-r1-{debug,release}-{build,ctest}.log`。没有重跑完整套件，没有修改 Gallery/global docs/goldens 或 root 的未提交内容；只作本地英文提交，不 push、t3 remote、PR、archive 或创建子任务。

## Astra integration R2：Select topmost wheel ownership（2026-10-24）

从同一干净本地分支 `e5c3a4a75918a9546bf4a9ff2acbe6b6deec9002` 继续，没有合并 root main。先仅增加测试、保持生产代码不变，以下四个独立 Debug case 均 exit 1：

| case | 实际失败断言 |
| --- | --- |
| `wheel-sibling-select-short` | `sibling Dropdown boundary/padding wheel scrolled covered Select` |
| `wheel-sibling-select-boundary` | `sibling Dropdown boundary/padding wheel scrolled covered Select` |
| `wheel-sibling-select-padding` | `sibling Dropdown boundary/padding wheel scrolled covered Select` |
| `wheel-select-topmost` | `Select mount order overrode topmost popup wheel ownership` |

前三个 case 先显示八个 options、`listHeight(96)` 的 Select，再打开 sibling Dropdown，设置 `Dropdown.z_index_popup=2000`，分别覆盖单项菜单、长菜单滚动下限与 popup padding。测试确认实际 topmost hit 属于 Dropdown row/padding，wheel 不改变被遮挡的 Select；露出的 Select popup 仍能滚动，双方 retained rows 不变并完整清理资源。

第四个 case 检查相同 Select handler 的相关 owner 路径：较早 mount 的高 z-index Select popup 覆盖较晚 mount 的 sibling Select，真实 topmost popup 必须先滚动；其 padding 与滚动边界不能传给下面的 Select，下面露出的区域仍能独立滚动且 retained rows 不变。

生产修复仅在 `src/component/select_component.cpp::on_scroll_input` 增加 topmost hit 的最近 window-layer root 检查，必须与实际可见且 active 的 Select popup 匹配，随后才使用原 bounds/scroll 逻辑。这样 sibling 或 child popup 的归属不再由几何重叠和 mount 顺序决定；没有重排 host、改动 child Select 优先级或增加通用路由框架。

相关 scroll-owner 源码检查：Menu 在最近 window layer 边界停止，Tabs 已比较 hit/window layer，TextArea 从 topmost hit 路由，Spin 检查实际 shield hit。Dropdown R1 fallback 已按 topmost popup 子树消费本次边界事件；Table/Modal 位于这些 popup handlers 之后。本次具体 Dropdown/Select 事件由所属 popup 消费，保留 Table/Modal 的正常祖先滚动合同。既有 child Select/nested Dropdown 优先级、四个 Dropdown/Table 回归、inline Menu → Modal handoff、Tabs 与 TextArea 的实际合同均在本轮聚焦测试通过。

使用 MSVC 19.51.36256 / Ninja Multi-Config / `windows-msvc-headless`，Debug/Release 顺序构建，每次最多 4 compile jobs。targets 为 R1 的六个 executable，加 `rynui_portable_tabs_component`、`rynui_portable_text_area`；两种 configuration 使用相同 CTest filter：

```text
^rynui\.(table_component|portable\.(menu_component|dropdown_component|select_component|modal_component|modal_boundary|tabs_component|text_area))$
```

- Debug：**8/8**，2.97 秒；Release：**8/8**，1.56 秒。Dropdown executable 全量运行，包含本轮四个新增 case 与 R1 四个 Table case。
- clang-format 22.1.3 检查 **560** 个自有源文件，0 failures；doctor healthy；strict validate **69/69**；diff check 通过。
- 本地原始记录：ignored `out/integration-r2-reproductions.json`、`out/integration-r2-{debug,release}-{build,ctest}.log`。没有重跑完整套件，没有改 Theme bridge、goldens、Gallery/global docs 或 root，只作本地英文提交；没有 push、remote 操作、PR、archive 或创建子任务。
