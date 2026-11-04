# Transfer main 集成结果

- 从干净的 `2777cdc3` 仅合入 `bc790b8c`、`560874ce`、`63eef292` 三个新增量；不重放 partial 基线，不改动 Cascader 实现。源工作树未修改、未推送；既有 29 项完成记录保留，仅关闭 23.1/23.2，进度 **31/135**。
- 实际平台：Linux GCC 16.2.1、Ninja Multi-Config、BUNDLED，所有 build 使用 CMake presets 与 `-j2`。
- `linux-gcc-headless` Debug 的 27 个受影响测试均取得通过结果（首轮 26/27，修正新增测试的 Select retained-row 假设后 Pagination 1/1 复验通过）；Release **27/27**，最终未解决失败 **0**。Transfer 基线 **6** 场景及 complete **15** 合同组均通过；没有执行全量 CTest 或旧导航 GPU 矩阵。
- 组合范围包含 Transfer/Theme、Cascader/ColorPicker Theme 共存、Pagination/现行 Select、Input/variant/TextArea/selection/focus、custom Table/action、Gallery 页面/源码/目录、公开 Theme 与 Core/backend 边界。新增 Pagination 回归验证 locale/RTL/showLessItems、页大小拒绝/接受、quick jumper 拒绝/接受及非法输入、动态 simple/方向更新和保留身份；五个 Transfer Theme accessor 均验证外线程拒绝。
- 首轮新增 Pagination 测试的未显式类型初始化导致编译失败，改为 `vector<size_t>` 后构建通过；首轮测试又误将保留空状态行计入选项数，改用现行 `total_rows/row_indices` 后复验通过，没有删减功能或降低选择/回调断言。
- `linux-gcc` Debug **4/4**、Release **4/4**：SDL event adapter、platform input、event pump、platform lifecycle。修复同一 SDL pump 内读取最终 modifier 状态造成 Shift 元数据时序错误：pump 前保存起点，按 key event 更新，失焦/批次 clear 清理；回归覆盖事件顺序与旧 PointerInputEvent 聚合初始化。此证据不等同于物理键盘实测。
- 五份完整 Theme golden 从合并代码重生成；`verify_transfer_theme_baseline.py` 对照 `2777cdc3` 的 **60** 个原有顶层字段，除 identity 外值与结构全部相同，仅新增 Transfer。TokenIdentity 及 names 顺序对应；Transfer 保留 **17 colors / 13 metrics / typography / effects / order**，36 字段域合同通过。
- 源生成器实算 **87** 组源码样例，Transfer 页 **3** 个实际实例；**458** frozen legacy IDs、归属清单及 source manifest 与 main 基线逐字一致，所有既有导航和数据录入样例保留。host inventory 对照 generated source registry。

- `linux-gcc` Release Gallery 已重建；只经 `run_gallery_page_acceptance.py` → `run_background_wayland.py` 运行 **Transfer → Cascader → Pagination → Button：4 cases / 401 checks / 84 submitted frames，最终失败 0**。真实 Vulkan/SPIR-V、system fonts、host input area/cursor sync 均通过；84 个 compositor frame callbacks 与 action pacing、正常 SDL pump 保留，无桌面 fallback。
- 原生序列实际执行行移动及回调、受控拒绝再接受、Shift pointer 元数据范围选择、locale/RTL/动态几何、小数滚轮、Token header GPU pixel、富行草稿移动、自定义 Table 选择与 action、footer TextArea draft/preedit/capture/session 离页清理，以及当前 Pagination Select/quick jumper 与完整 Cascader 的组合回归。
- 首次四页顺序为 Transfer → Pagination → Cascader → Button，在 Compact 字号的 Cascader 中三个标签全部可见，触发原验收驱动“必须溢出”假设（2 cases 完成、320 checks、68 frames，失败 1）。改用上述等价顺序，让 Cascader 在常规字号下执行相同断言后通过；未改动 Cascader 实现、样例或既有断言。
- clang-format **22.1.3** 检查 **927** 个自有源文件，0 failures；catalog `--check --self-test`、源码生成 `--check`、doctor healthy、strict **107/107** 和 `git diff --check` 通过。

必要命令（HEADLESS Debug/Release 分别构建受影响 targets 并运行；原始日志仅在忽略的 `out/transfer-integration/`）：

```sh
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-debug --target rynui_portable_transfer_component rynui_portable_transfer_complete rynui_transfer_theme_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests rynui_cascader_theme_tests rynui_color_picker_component_tests rynui_pagination_component_tests rynui_portable_select_component rynui_select_complete_tests rynui_auto_complete_complete_tests rynui_portable_input_component rynui_portable_input_variant rynui_portable_text_area rynui_portable_selection_component rynui_portable_focus_lifecycle rynui_portable_theme_scope_component rynui_portable_theme_public_api rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests rynui_ant_design_reference_catalog_tests rynui_gallery_pages_host_tests -j2
ctest --preset linux-gcc-headless-debug -R '^(rynui\.(transfer_theme|cascader_theme|color_picker_component|theme_algorithm|theme_runtime|pagination_component|select_complete_(window|retained|lifetime)|auto_complete_complete_(window|form_editor)|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog|gallery_pages_host|gallery_color_number)|rynui\.portable\.(transfer_component|transfer_complete|select_component|input_component|input_variant|text_area|selection_component|focus_lifecycle|theme_scope_component|theme_public_api|backend_boundaries))$' --output-on-failure
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release --target rynui_sdl_event_adapter_tests rynui_platform_input_tests rynui_platform_event_pump_tests rynui_platform_lifecycle_tests -j2
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
ctest --preset linux-gcc-release -R '^rynui\.(sdl_event_adapter|platform_input|platform_event_pump|platform_lifecycle)$' --output-on-failure
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page transfer --page cascader --page pagination --page button --timeout 120 --log-dir out/transfer-integration/native-combined
python tests/verify_transfer_theme_baseline.py
python tools/generate_gallery_source.py --output out/build/linux-gcc-headless/gallery-generated/generated_gallery_source.inc --check
python tools/update_ant_design_gallery_catalog.py --check --self-test
python scripts/format-code.py --check --clang-format out/linux-validation/format-tools/bin/clang-format
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

未验证 Windows/MSVC/D3D12、系统 IME 候选窗、物理键盘、OS 辅助技术、实际桌面或跨显示器体验。整个数据录入分类尚未完成，63.2/64.2/65.1 保持未勾选；不把本轮局部平台结果扩大为分类完成。原始 log/JSON/图片/二进制及生成源码只留在忽略的 out。
