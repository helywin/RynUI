# Menu 平台通用验收

- 实际环境：Linux x64 / GCC / Ninja Multi-Config；`linux-gcc-headless`、RECORDING，Debug 与 Release。
- 配置采用 linux-gcc-headless preset；四个既有文本依赖使用本工作树 out 下的 BUNDLED source cache，避免本机首次 FreeType 下载的 TLS 失败。
- 最终结果：两个配置各 6/6 CTest 通过；Menu 8 组、受影响 Dropdown 27 组，另含 Theme algorithm/runtime、Gallery 页面和目录合同。Gallery definition/sample/catalog 前端 target 在两个配置构建成功。
- 覆盖稳定 key 增删/重排/跨父项移动、富标签 Input 身份与隐藏清理、受控拒绝、多选、三种模式/RTL、溢出/嵌套 flyout、最深层 Escape、wheel/键盘滚入、延迟取消、回调销毁/异常和闲置清理。InlineCollapsed 提供窄栏、居中图标/Unicode 首字素回退，显式外部宽度仍由应用决定。
- 保留原 Menu 构造、items(vector)、onClick/onSelect 与 Breadcrumb/Tabs/Dropdown 调用；新增 Prop items/mode/theme/direction/inlineCollapsed、MenuLabel/MenuExtra 和 hover delay。内部 helper 为 `MenuComponentHost::close_deepest`，`MenuSnapshot::colors` 提供实际应用的 palette 诊断。
- Menu metrics 保持原顺序并追加 collapsed_width/collapsed_icon_size（7 → 9）；light_colors/dark_colors 在 ThemeSnapshot 中按 scoped seed/alias/Token 解析，variant-only 变化同样触发 menu_colors invalidation。五份完整 golden 保留此前所有字段，追加 palette 与折叠 metrics，未删减 expected shape。
- 上游对照：Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Menu API、interface、实现与 horizontal/inline-collapsed demo。源码副本与运行输出仅在忽略的 out。

实际命令：

```text
cmake --build --preset linux-gcc-headless-debug -j2 --target rynui_menu_component_tests rynui_dropdown_component_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests
cmake --build --preset linux-gcc-headless-release -j2 --target rynui_menu_component_tests rynui_dropdown_component_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests
ctest --preset linux-gcc-headless-debug -R '^rynui\.(menu_component|dropdown_component|gallery_sample_page|ant_design_reference_catalog|theme_algorithm|theme_runtime)$'
ctest --preset linux-gcc-headless-release -R '^rynui\.(menu_component|dropdown_component|gallery_sample_page|ant_design_reference_catalog|theme_algorithm|theme_runtime)$'
```

目录生成/check、clang-format 22.1.3（876 个自有文件、0 失败）、OpenSpec doctor healthy、strict 107/107 与 diff 检查通过。仅更新 Menu/Dropdown overlay，保留 458 个 frozen legacy IDs；新增目标使用普通 Gallery 注册。首轮 Release 暴露的 RTL 标签/箭头重叠已修复，最终上述检查全部通过；本机 Ninja 曾恢复依赖缓存，结果取自重建后的二进制。

尚未验证：原生 Gallery 应用/GPU、driver/system font/DPI、实际桌面/系统 IME 与 Windows/MSVC；本阶段没有运行原生窗口。63.1、64.*、65.1 保持父线程负责；未将 HEADLESS 结果计作这些平台项通过。Core/renderer 边界与 GPU ABI 未更改。
