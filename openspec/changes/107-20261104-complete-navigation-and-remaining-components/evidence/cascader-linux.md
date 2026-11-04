# Cascader Linux 完成结果

本隔离分支基于 `29faf58d4c862e3bf641fb5c47875f30187440b6`，完成 change107 的 Cascader 9.1/9.2。合同锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；API 与保留工厂语义见 `docs/components/cascader.md`。Transfer 仍为导入的 partial 基线，23.* 未关闭。

- 平台与构建：Linux GCC 16.2.1，`linux-gcc-headless` / `linux-gcc`，Ninja Multi-Config，全部构建 `-j2`。
- HEADLESS Debug **13/13**、Release **13/13**：Cascader（20 组场景）、Cascader Theme、完整 Theme golden/运行时、Input/variant、Button、焦点生命周期、作用域 Theme、Gallery 页面/源码/目录与 Core/backend 边界。未运行全量 CTest；目录提升后 Gallery/源码/目录子集在 Debug/Release 各再通过 3/3。
- Theme：独立 palette/metrics/typography/effects、seed/algorithm/inherit、完整 snapshot/identity；五份 golden 保留基线全部原有字段和值（identity 因新增域更新），包括 Menu light/dark/collapsed、Dropdown arrow、Anchor、Layout/Splitter/FloatButton。实际行布局、按钮颜色与 selector glyph 行高变化均验证不重建无关工厂。
- 交互：真实保留 tags/单项删除/受控拒绝/custom tag draft、responsive 与交互溢出；稳定布局不持续请求帧。独立列/搜索头/状态区裁剪、Unicode folding、filter/sort/render/limit、键盘与精确滚轮、窄/零区域、RTL、富子控件显露和动态内容已覆盖。
- 生命周期：selector/search/富选项/子浮层的 Core preedit/session；Escape 最内层优先、Tab/ShiftTab、disabled/readOnly/hidden/blocked/零面积/窗口失活、焦点与 capture 移除、回调销毁/抛错、append_slot 失败与窗口销毁。渲染重入采用完整组件退场，其他组件保持健康。
- Loader：搜索/新加载/选项替换/隐藏/卸载/窗口销毁拒绝旧票据，重复完成拒绝，8 个外线程共 400 次完成请求拒绝；回调重入/抛错、错误/真实 Retry、完成后的闲置状态均覆盖。完成仍须由应用派发回 owner thread。
- Gallery：7 个实际 Cascader 示例，包含 tags、独立 Theme、长列搜索/滚动、本地错误/Retry、自定义 tag/触发器/空状态。实际源码生成器输出 **81** 组样例，冻结 **458** 个 legacy ID 与基线完全一致；这是本分支计数，父线程合入更新的 main 后应重算。
- 原生 Release：私有 Wayland 的 Cascader + Button 两页通过 **2 cases / 84 checks / 16 submitted frames**；实际 Vulkan、SPIR-V、系统字体、host input area/cursor 同步，无 desktop fallback。Cascader 真实执行 tag 删除、loader 错误与 Retry、Unicode 搜索、小数滚动、Core preedit 和离页资源清理。

HEADLESS 的实际构建与测试命令（对 Debug/Release 分别执行）：

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-debug --target rynui_portable_cascader_component rynui_cascader_theme_tests rynui_theme_algorithm_tests rynui_portable_input_component rynui_portable_input_variant rynui_portable_button_component rynui_portable_focus_lifecycle rynui_portable_theme_scope_component rynui_theme_runtime_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_gallery_source_panel_tests -j2
cmake --build --preset linux-gcc-headless-release --target rynui_portable_cascader_component rynui_cascader_theme_tests rynui_theme_algorithm_tests rynui_portable_input_component rynui_portable_input_variant rynui_portable_button_component rynui_portable_focus_lifecycle rynui_portable_theme_scope_component rynui_theme_runtime_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_gallery_source_panel_tests -j2
ctest --preset linux-gcc-headless-debug -R '^(rynui\.(cascader_theme|theme_algorithm|theme_runtime|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog)|rynui.portable.(cascader_component|input_component|input_variant|button_component|focus_lifecycle|theme_scope_component|backend_boundaries))$' --output-on-failure
ctest --preset linux-gcc-headless-release -R '^(rynui\.(cascader_theme|theme_algorithm|theme_runtime|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog)|rynui.portable.(cascader_component|input_component|input_variant|button_component|focus_lifecycle|theme_scope_component|backend_boundaries))$' --output-on-failure
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page cascader --page button --timeout 120 --log-dir out/cascader-continuation/native-final
```

目录/源码生成 `--check`、catalog `--check --self-test`、clang-format 22.1.3（898 个自有文件，0 失败）、doctor healthy、strict 107/107 与 `git diff --check` 通过。BUNDLED 依赖使用本地锁定源码缓存，未改依赖模式或锁。

未验证边界：Windows/MSVC、系统 IME 候选 UI、辅助技术及真实桌面/跨显示器体验。Core 注入的 composition 和后台 GPU 结果不替代这些平台证据。63.2/64.2/65.1 保持父线程汇总任务；未新增人工 checkbox。原始日志、源码冻结副本及运行结果全部留在已忽略的 `out/`，仓库只保存本摘要。
