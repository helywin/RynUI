# 导航首批 Linux 整合

- 整合基线 `d0ea6830`，来源本地提交 `91fa6f3e`（Anchor）、`3bd2f3ce`（Breadcrumb）；保留 main 已完成的 Menu/Dropdown。未修改来源工作树或推送 T3 分支。
- 五份完整 Theme golden 由合并后的 `rynui_theme_algorithm_tests --write-goldens` 生成；逐字段对比基线，原有 57 个顶层字段除整体 identity 外完全相同，新增 Anchor 后 58 个字段。Menu light/dark palettes、9 项 metrics、Dropdown 7 项 metrics（含 arrow）全部保留。新增 TokenIdentity 名称/颜色分域失效回归。
- source registry 实际为 79 个样例，catalog generator/check/self-test 通过；新增目标走实际页面注册，冻结 legacy inventory 保持原有 458 项逐项不变。共享 Gallery 注册仍位于 HEADLESS early return 前。
- 实际平台：Linux x64 / GCC / Ninja Multi-Config。`linux-gcc-headless` Debug/Release 聚焦组合最终各 18 个唯一 CTest 通过，含 Anchor/Breadcrumb/Menu/Dropdown、Theme/runtime、目录/source、public umbrella 示例、Recording 导航与 portable backend boundary。Debug 初轮 16/17，旧 inventory 对象导致唯一失败；增量重建后该项及 runtime/编译示例 3/3 通过。演示收窄以实际触发 overflow 后，两个配置的 sample/source 再各 3/3 通过。

构建 target：`rynui_anchor_component_tests rynui_breadcrumb_component_tests rynui_menu_component_tests rynui_dropdown_component_tests rynui_navigation_completion_gallery_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests rynui_ant_design_reference_catalog_tests rynui_gallery_sample_page_tests rynui_token_gallery_frame_tests rynui_navigation_examples_tests`。使用 `cmake --build --preset linux-gcc-headless-<debug|release> -j2 --target <以上 targets>`。

```sh
filter='^rynui\.(anchor_component|anchor_gallery|breadcrumb_component|breadcrumb_completion_gallery|menu_component|dropdown_component|theme_algorithm|theme_runtime|ant_design_reference_catalog|gallery_sample_page|ant_design_gallery_catalog_(generator|contract)|gallery_sample_source_(contract|generated)|portable.backend_boundaries|gallery_page_navigation_frame|navigation_docs_examples|navigation_examples)$'
# Debug 首轮 filter 不含 navigation_examples，增量重建后运行：
ctest --preset linux-gcc-headless-debug -R '^rynui\.(gallery_sample_page|navigation_examples|theme_runtime)$' --output-on-failure
ctest --preset linux-gcc-headless-release -R "$filter" --output-on-failure
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page anchor --page breadcrumb --page menu --page dropdown --page button --timeout 90 --log-dir out/change107/navigation-partial-integration/native-overflow
```

实际私有后台 Wayland 序列 **5/5 cases、352 checks、72 submitted frames 通过**；Vulkan/SPIR-V、system fonts，host input-area/cursor 服务均有运行证据，无桌面 fallback。Anchor 实际正文滚动/层级/拓扑；Breadcrumb 保留编辑草稿、RTL、内置新版 Menu/Dropdown 真实回调和离页清理；Menu 动态重排/添加、横向 overflow、RTL flyout 回调、折叠和带浮层销毁；Dropdown 受控选择关闭、split 主按钮、hover/箭头、嵌套 flyout 回调、自定义 popup 关闭重开保留草稿及离页 popup/editor/session/capture 清理，均有专项动作。保留既有 frame callback/pacing 与 input-first wheel 路径。

早期原生运行暴露 runner 对 Menu overflow 内部行计数的错误假设，以及 Compact 下原样例宽度未产生 overflow；已修正计数、在检查前用真实 wheel 显露目标，并收窄该演示后通过。原始日志/JSON 仅在忽略的 out。

clang-format 22.1.3 检查 884 文件、0 失败；OpenSpec doctor healthy，strict 107/107，diff 检查通过。1.1–4.2 保持完成。63.1 等待 Pagination/Steps/Tabs 的后续整合，Windows 64.*、最终 65.1 保持开放。

边界：窗口真实使用 Linux Wayland/Vulkan/system fonts，动作由有限宿主合成 Pointer/Key/Text 及实际 Gallery wheel 分发驱动；不等于物理设备、实际桌面、跨显示器、系统 IME 候选窗口或辅助技术实测。未运行 Windows/MSVC。本结果只覆盖首批四个导航组件，未宣称整个 change 完成。
