# Dropdown 平台通用实现阶段

- Linux x64 / GCC / `linux-gcc-headless` / Debug / RECORDING。
- `cmake --build --preset linux-gcc-headless-debug -j2 --target rynui_menu_component_tests rynui_dropdown_component_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_theme_algorithm_tests` 成功。
- `ctest --preset linux-gcc-headless-debug -R '^rynui\.(menu_component|dropdown_component|gallery_sample_page|ant_design_reference_catalog|theme_algorithm)$'`：5/5 通过；Menu 8 组、Dropdown 27 组（原有 23 组 + 新增 4 组）。
- 新增覆盖四种 hover 组合、延迟/取消/失活/受控拒绝、来源 cause、split 原生回调/loading gate、保留自定义 Input/默认 Menu/子 Dropdown、焦点与文本会话清理、回调销毁、factory 异常、12 方向箭头/溢出及零尺寸箭头闲置。
- 保留 `Dropdown(props, trigger)`、Menu 默认行为、`source` 枚举和现有回调签名；新增 typed popupRender、DropdownMenuContent、DropdownButtonProps/Slots。DropdownOpenInfo 新增 cause，两字段 aggregate 初始化仍可用，结构化绑定需接收第三字段或改具名访问。
- `DropdownMetricToken::values()` 末尾新增 arrow_size（6 → 7 字段），独立 `ThemeConfig::dropdown.tokens.arrow_size`；使用共同 logical TooltipArrowPath 联合填充，未新增 renderer ABI/SDL 路径。五份 golden 由现有 writer 完整生成；逐字段比较仅 dropdown.metrics 长度与 identity 变化，其他字段完整保留。
- 配套修复 Menu 的初始隐藏/再次隐藏浮层边界、动态 slot 创建失败、跨父项移动及 flyout priority 溢出检查；不改全局输入优先顺序或 hit-test 路径。
- 上游依据为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Dropdown API、实现、dropdown-button 类型/实现与 custom-dropdown、dropdown-button demo；其 Web Dropdown.Button 已弃用，本地 helper 使用 Button + SpaceCompact 原生组合。
- Release、最终支持清单/生成目录验收继续在 3.2/4.2 追踪。原生 Gallery/GPU、Windows/MSVC、系统字体/DPI/IME 未在本阶段运行；不勾选 63.1/64.*/65.1。
- clang-format 22.1.3：876 个自有文件、0 失败；OpenSpec doctor healthy、strict 107/107、diff 检查通过。

补充实现阶段：DropdownMenuContent 增加 popup weak lifetime 与 window/owner generation 检查；保存的内容在窗口销毁后调用会被拒绝。对应定向 Debug 为 6/6 CTest，Dropdown 27 组全部通过；Release 继续由 3.2 追踪。
