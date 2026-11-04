# 导航 Linux 组合验收

在已推送的 `6c08ea5c`（Anchor/Breadcrumb 与 Menu/Dropdown 整合）上，整合本地 `b4f37cfb`（Pagination）、`12773f64`（Steps）、`dfad492c`（Tabs）。未改动来源工作树、未推送 T3 分支。各组件平台通用范围见对应 `*-linux.md`；首批组合见 [navigation-partial-integration.md](navigation-partial-integration.md)。

- 实际环境：Linux x64 / GCC / Ninja Multi-Config；`linux-gcc-headless` Debug/Release，构建均 `-j2`。本阶段聚焦组合各 **21/21 CTest 通过**，覆盖三个组件、Tabs model/theme/public API、三页 Gallery、目录/source、public umbrella 编译示例、3 个 Recording 页面场景和 portable backend boundary。Gallery 演示调整后 Debug 再执行三页/sample/source **6/6** 通过；未重复完整 CTest。
- 新增 Steps Inline 嵌套 Popover 的真实焦点/输入回归：修复前首个 Escape 错误关闭外层，修复后首个 Escape 只关闭子 Popover，第二个关闭外层说明并清理输入会话。复用现有 `PopoverComponentHost::owns_escape`；未改变公开 API。
- Tabs 保留默认 premount，显式 lazy/destroyOnHidden 与 keyed rich factory 身份继续有效。实际新版 Menu/Dropdown 参与 Tabs overflow；共同 NavigationTransition 的有限运动在组件合同与原生 Steps/Tabs 动作中验证。
- 当前 79 个真实 source/sample registry、458 个冻结 legacy IDs；新组件示例只使用普通页面 target。共享 Gallery/新 Tabs 注册在 HEADLESS early return 前。此批 Theme golden 没有修改，首批合并的完整字段保持原样；Core/renderer 边界与 GPU ABI 未更改。

构建 target：`rynui_pagination_component_tests rynui_steps_component_tests rynui_tabs_model_tests rynui_tabs_theme_tests rynui_tabs_public_api_tests rynui_tabs_component_tests rynui_navigation_c_gallery_tests rynui_ant_design_reference_catalog_tests rynui_gallery_sample_page_tests rynui_token_gallery_frame_tests rynui_navigation_examples_tests`，命令为 `cmake --build --preset linux-gcc-headless-<debug|release> -j2 --target <以上 targets>`。对应聚焦 CTest：

```sh
filter='^rynui\.(pagination_component|steps_component|tabs_(model|theme|public_api|component)|pagination_completion_gallery|steps_completion_gallery|tabs_completion_gallery|ant_design_reference_catalog|gallery_sample_page|ant_design_gallery_catalog_(generator|contract)|gallery_sample_source_(contract|generated)|portable.backend_boundaries|gallery_page_(navigation|data|select-card-tabs)_frame|navigation_docs_examples|navigation_examples)$'
ctest --preset linux-gcc-headless-debug -R "$filter" --output-on-failure
ctest --preset linux-gcc-headless-release -R "$filter" --output-on-failure
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page anchor --page breadcrumb --page menu --page dropdown --page pagination --page steps --page tabs --page button --timeout 120 --log-dir out/change107/navigation-integration/native-variants
```

实际私有后台 Wayland 序列 **8/8 cases、643 checks、131 submitted frames 通过**，Vulkan/SPIR-V、system fonts，host input-area/cursor 服务均为 true，无桌面 fallback。保留 frame callback 与有限 action pacing，未以单纯挂页代替专项行为：

- Anchor/Breadcrumb/Menu/Dropdown：重新通过实际滚动、保留草稿/键控结构、RTL、受控开合、flyout/hover/split/custom popup 回调及离页清理。
- Pagination：通过原生 Select 改页大小、实际文本编辑/Enter 跳页、公开 onChange 反馈、simple 输入与响应式 compact、离页输入清理。
- Steps：实际编辑草稿、键控重排、选择回调反馈、Dot/Inline/Navigation/Panel、Inline 编辑器 Escape 清理、有限动画结束与离页释放。Dot 演示 maxCount 折叠范围，其余变体展示全部条目，runner 只激活可交互项。
- Tabs：实际编辑与重排保留草稿、lazy 挂载及 destroyOnHidden 销毁、onEdit 新增/删除回调、窄宽 overflow Dropdown/Menu 选择切换、左右/RTL、有限动画结束、popup/editor/session/capture 离页清理。

早期原生运行曾错误尝试激活 maxCount 的省略项，已修正演示与 runner 后完整序列通过。原始日志、JSON 和图像仅在忽略的 out。

clang-format 22.1.3：886 文件、0 失败；目录 generator/check、自测、source 校验通过。OpenSpec doctor healthy、strict 107/107、`git diff --check` 通过。导航任务 1.1–7.2 与 Linux 63.1 完成；Windows 64.1、其余平台分类和跨组件最终 65.1 保持开放。

边界：这是实际 Linux Wayland/Vulkan/system-font 运行中的有限宿主合成 Pointer/Key/Text 与实际 Gallery wheel 分发。未操作用户桌面或物理设备；不代表跨实际显示器、系统 IME 候选窗口、系统辅助技术或 Windows/MSVC 实测。Release Gallery 位于 `out/build/linux-gcc/examples/Release/rynui_token_gallery`。本次只收口导航，不宣称整个 change 完成。
