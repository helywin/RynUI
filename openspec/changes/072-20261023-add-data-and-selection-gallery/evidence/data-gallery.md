# 数据与选择 Gallery 验收

## 聚焦场景（2026-10-24）

- 基线 main：`c931b2e3b304651d98e8b2ecc5bec8e9558a0a1c`，已包含 Table/Pagination、Menu/Dropdown 和 InputNumber 集成。
- 实际环境：Windows x64、MSVC、Ninja Multi-Config，`windows-msvc` Debug；Recording fixture 使用真实组件、字体与场景同步，未创建原生窗口。
- 构建 targets：`rynui_token_gallery`、`rynui_token_gallery_frame_tests`。新增聚焦入口 `rynui_token_gallery_frame_tests.exe --data-only` 通过。
- Table：实际单元格数字编辑及源数据回写、文字更新、升序/降序/无排序、Checkbox 选择、与 Pagination 双向页码联动、loading 禁止排序、数据删除/重排/恢复；同 key 行与 editor 身份保留，翻页保留编辑值与选择。
- Menu/Dropdown：整页滚动后实际指针选择、响应式标签、子菜单展开收起、禁用切换、Home/Enter 导航；点击与右键触发弹层，执行清空 Table 选择命令，Escape 关闭。
- InputNumber：受控小数步进和编辑/提交、InputRef 聚焦、禁用解除后步进、只读禁止变更；普通更新不重新执行 Gallery 或 Theme 内容。
- 更新五项 support overlay 为 `partial`，保留各组件首版缺口及独立平台验收边界，重新生成锁定 6.6.5 目录。

实际 inventory：251 stable IDs、93 navigation controls、297 Buttons、875 interactions、557 visible interactions、269 live samples、67 Theme content runs、131 Inputs、20 Selects、13 Modal/Drawer、45 selection controls、2 Menus、1 Dropdown。新增 Table 的 12 行编辑器全部保留，仅当前页参与布局与输入。

目录/生成器、document/viewport contracts、token Gallery contract、Windows acceptance runner contract 与 evidence schema：CTest **8/8**，0.49 秒。clang-format 22.1.3 检查 560 个自有源文件，0 failures；doctor healthy；strict validate **70/70**；diff check 通过。

本地原始输出位于 ignored `out/data-gallery-{focused,inventory,catalog,format,doctor,validate}.log`。完整 `token_gallery_frame` 属于任务 2.2，以下另行记录；这里不把编译或聚焦场景当成完整回归，也不宣称原生 GPU、系统 IME、人工视觉或 Linux 验收。

## 完整共同回归（2026-10-24）

- main `9f553ea8b27f5d98a89e5109288731e5ffb5d34d`，同一 MSVC `windows-msvc` Debug preset，重新构建 Gallery/frame executable。
- 首次完整回归发现 `gallery.password.toggle-action` 高度为零。新增目录支持说明和样例使窄屏整页内容超过原先测试用的 40,000 logical px 上限；实际 900px 文档滚动场景已通过。把仅供完整目录绘制的测试视口增加为 60,000px，保留真实滚动/输入场景的 900px 视口与所有可达性断言，并增加具体按钮标签/尺寸的失败诊断和 `--frame-only` 聚焦入口。
- `--frame-only` 通过；随后完整 `rynui.token_gallery_frame` **160.02 秒通过**，包含宽窄屏、主题/品牌/字体切换、导航/过滤、滚动/输入与新数据场景。与目录/生成器、document/viewport、token/evidence contracts 合计 **9/9**，160.52 秒。
- clang-format 560 文件、doctor、strict validate **70/70** 和 diff check 再次通过。原始日志：ignored `out/data-gallery-full.log`、`out/data-gallery-frame-only.log`。

## Windows 原生烟雾测试

在 `0554019983a3410fa579c2ec7f8547df100f48f8` 的新数据 Gallery 构建上运行 `rynui_token_gallery.exe --smoke`，实际 `direct3d12` / `DXIL`、display scale 1.5；251 stable IDs、269 live samples，7 个 frame samples，`exit_code=0`。原始输出为 ignored `out/data-gallery-native-smoke.log`。

此项只确认真实窗口/GPU 路径的启动、自动主题切换与退出；没有人工逐组件视觉、系统 IME、完整 DPI 矩阵或 Linux 原生验收。Dropdown/Select 重叠浮层的后续审查修复属于组件集成任务，未用本 Gallery 烟雾结果代替其专门回归。
