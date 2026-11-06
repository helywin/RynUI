# Tag Linux 定向验收

这是隔离来源提交的验证记录；当前 main 的重新验证见 [Avatar/Badge/Tag 集成](avatar-badge-tag-integration.md)。
2026-11-05；隔离源 `t3/implement-avatar-badge-tag`，基线 `135ba70b`；固定 Ant Design 6.6.5 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。`CheckableTagGroup` 对应真实上游导出；`TagGroup` 对应 draggable demo 的原生 retained 组合，以独立手柄保护富 Input 选择。

- Linux GCC 16.2.1 / BUNDLED / HEADLESS/Recording / Ninja Multi-Config，`linux-gcc-headless-debug`、`linux-gcc-headless-release`，最多 `-j2`。共同集合 **57 个 CTest 项在 D/R 全部通过**，Release 首轮漏建目录 executable 的 1 Not Run 已补建并仅重跑通过；最终 Tag 增补和目录刷新后的 Debug **7/7**。完整计数/选择器见 [共同结果](avatar-linux.md)。
- Tag native 最终 D/R 各 **43 条断言**，不是 43 个 CTest 项：有限 enter/close midpoint、close 取消/接受、显式 visible 受控拒绝/接受、defaultChecked、关闭立即撤销 Input focus/preedit/session、过期文本拒绝、保留/退役边界、close 自销毁/throw 与 deadline 清理。
- 实际 Input draft/ref/selection 经稳定键重排、同键数据修改保留；另一键工厂 revision 只替换该键，删除也保留兄弟编辑器。动态 add/remove/reorder、factory 中重入发布最新 items、重复 key 原子拒绝、factory 挂载后抛出准确退役 owner（独立兄弟仍活）均覆盖。
- 真实 PointerRouter 捕获、5dp 阈值后的实际几何位移、disabled 取消、精确第三项落点重排、空白 clip 区域不重排与释放捕获；键盘受控请求拒绝/接受、reduced-motion 关闭 roving focus 转移均覆盖。修复 drop 只检查祖先 clip 的问题，最终同时检查 translated bounds 与 clip。
- 私有 Tag 内容不抢外层 Form 自动绑定；显式嵌套 FormItem 正常绑定，外层编辑保持权威，连续三次 Form reset 不改私有 draft。复用 `FormPrivateEditorScope`，没有私有 selector/Enter 权威别名。
- 独立 Tag Gallery 使用可取消关闭、受控 CheckableTag/CheckableTagGroup、接受/拒绝拖排和保留富草稿，反馈来自真实 callback；三页 Recording **3/3 CTest**，串行 all 模式 D/R 各 **38 检查**。

最终增补实际命令（Release 构建相同 native/API target 并纳入共同 57 项）：

```text
cmake --build --preset linux-gcc-headless-debug --target rynui_tag_native_contract_tests rynui_ant_design_reference_catalog_tests -j2
ctest --test-dir out/build/linux-gcc-headless -C Debug --output-on-failure -R '^rynui\.(tag_native_contract|ant_design_reference_catalog|avatar_native_gallery|badge_native_gallery|tag_native_gallery|gallery_sample_page|gallery_source_panel)$'
```

新旧 public typed examples 的 **3 targets** D/R 构建通过；新例使用真实 Signal，无 `ryn::state`。目录/source/格式/OpenSpec 检查见共同结果。原 **93+3=96 samples/458 frozen IDs**、五份完整 **64 字段** Theme 保留；父集成以 generator 合并主线 **97/66**，不能覆盖。

只关闭 26.1/26.2、27.1/27.2、42.1/42.2；历史 56 项不重开，63.3/64.3/65.1 保持父线程所有。本分支真实 GPU/原生窗口验收 **0 项**，Windows/MSVC/D3D12、物理 IME/AT 与实际跨显示器未验证；无人工 checkbox。原始产物全部仅 ignored out。
