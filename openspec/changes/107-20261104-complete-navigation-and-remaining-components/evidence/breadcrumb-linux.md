# Breadcrumb：Linux 定向结果

- 基线：`f20a77415d8f37c185f0de74dfe5f570cbbed06a`；对照 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Breadcrumb/BreadcrumbItem API、类型、overlay/separator demos。
- 实现响应 keyed items 的增删、重排和更新；保留未变富标题/图标及编辑草稿，每项分隔符/激活处理器，直接组合现有 MenuProps/DropdownProps，换行及 RTL。替换/删除清理 popup、focus、capture 和输入会话；旧 vector 调用继续可用。未修改 Menu/Dropdown 内部实现。
- `linux-gcc-headless` Debug/Release：`rynui.breadcrumb_component` 各 1/1（6 组）；Breadcrumb Theme、3 项子控件/焦点回归、5 项 input-block 生命周期、2 项文档/编译示例和动态 Gallery 合同均通过，本组件相关各 13/13。共享 Anchor/Theme/catalog/source 检查纳入本轮组合，各 21/21；既有 `rynui_token_gallery_frame_tests --navigation-only` 在两配置各 6/6 Recording 场景通过。
- 构建使用 Linux GCC / Ninja Multi-Config，`cmake --build --preset linux-gcc-headless-debug` 与 `linux-gcc-headless-release`，`-j2`，target 为 `rynui_anchor_component_tests rynui_breadcrumb_component_tests rynui_breadcrumb_theme_tests rynui_navigation_review_tests rynui_navigation_block_lifetime_tests rynui_navigation_examples_tests rynui_navigation_completion_gallery_tests rynui_theme_algorithm_tests rynui_ant_design_reference_catalog_tests rynui_token_gallery_frame_tests`。

组合 CTest 命令（两配置分别执行）：

```sh
filter='^rynui\.(anchor_component|anchor_gallery|breadcrumb_component|breadcrumb_theme|breadcrumb_completion_gallery|navigation_review_.*breadcrumb.*|navigation_block_lifetime_breadcrumb.*|navigation_docs_examples|navigation_examples|theme_algorithm|ant_design_reference_catalog|ant_design_gallery_catalog_(generator|contract)|gallery_sample_source_(contract|generated))$'
ctest --preset linux-gcc-headless-debug -R "$filter" --output-on-failure
ctest --preset linux-gcc-headless-release -R "$filter" --output-on-failure
```

- Gallery 保留冻结 legacy ID，新增独立动态 trail；真实菜单选择、键控重排/删除/恢复、保留输入、宽度和 RTL 控件通过。Source inventory 为 79 个样例，所有既有样例保留。
- `python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：884 个自有源文件、0 失败。目录 `--check/--self-test`、OpenSpec doctor、strict all（107/107）及 `git diff --check` 通过。

原生 `linux-gcc-release` Gallery 构建通过。2026-11-05 私有 Wayland 两页序列 **2/2 case、128 项检查、24 帧通过**，实际 backend 为 Vulkan/SPIR-V，使用 system fonts，`host_input_area`、`host_cursor_sync` 均为 true。Breadcrumb 检查了真实编辑文本、重排后草稿不变、原生菜单选择回调、RTL、root/popup/editor/session 离页清理。

```sh
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page anchor --page breadcrumb --timeout 90 --log-dir out/change107/native-final
```

早期运行的重复聚焦判断错误已在验收辅助函数修复；保留窗口激活、可交互资格与真实 wheel 显露，修复后重跑通过。`63.1`、`64.1`、`65.1` 保持父任务范围。

边界：router/href/params/DOM/CSS/React 不移植；Recording 不代表 GPU、系统 IME 候选窗口、实际跨显示器或辅助技术验收。原始输出仅在忽略的 `out/change107/`。
