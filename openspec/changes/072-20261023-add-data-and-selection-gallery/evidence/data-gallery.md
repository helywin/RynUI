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
