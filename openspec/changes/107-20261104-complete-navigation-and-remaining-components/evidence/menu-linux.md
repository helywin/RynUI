# Menu 平台通用实现阶段

- Linux x64 / GCC / `linux-gcc-headless` / Debug（RECORDING；非原生 GPU）。
- `cmake --build --preset linux-gcc-headless-debug -j2 --target rynui_menu_component_tests rynui_dropdown_component_tests`。
- `ctest --preset linux-gcc-headless-debug -R 'rynui\.(menu_component|dropdown_component)$'`：2/2 CTest 通过；Menu 8 组、旧 Dropdown 23 组。
- 已验证动态 key 重排/删除与捕获取消、富标签 Input 身份、受控拒绝、多选、RTL 三种模式、横向溢出、嵌套 flyout/Escape、延迟取消、回调销毁/异常、Theme 更新与闲置清理。
- 保留 Menu 构造、items(vector)、onSelect 的取消选择语义，以及 Dropdown/Breadcrumb/Tabs 的既有签名；新增响应式 items/mode/theme/direction/inlineCollapsed、MenuLabel/MenuExtra 和 hover delay。`close_deepest` 为组件宿主内部 helper，不是公开 SDK。
- flyout 背景与行作为同一个 Menu 所有的 retained window layers；行的稳定身份支持跨父项移动，viewport clipping 保持独立，未改变 GPU ABI/Core 依赖边界。
- 上游对照：Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Menu API、interface、menu 实现及 horizontal/inline-collapsed demo；下载源仅在忽略的 out 中。
- 4.2 的 Release、Gallery 和目录验证继续进行；本阶段不声称完成这些检查。Linux 原生 GPU/系统字体、Windows/MSVC 与人工桌面/IME 未在本阶段运行；63.1/64.*/65.1 由父线程追踪。
- 格式检查：clang-format 22.1.3，876 个自有文件、0 失败；OpenSpec doctor healthy、strict 107/107，diff 检查通过。
