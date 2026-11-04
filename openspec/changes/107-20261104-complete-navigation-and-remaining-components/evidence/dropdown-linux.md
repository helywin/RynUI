# Dropdown 平台通用验收

- 实际环境、完整 build/CTest 命令见[同批 Menu 验收](menu-linux.md)：Linux GCC HEADLESS / RECORDING / Debug 与 Release，各 6/6 CTest 通过；Dropdown 27 组（原 23 + 新增 4），Menu 8 组及 Theme/Gallery/目录组合回归通过。
- 新增四种 hover 组合、延迟/取消/失活/受控拒绝、Trigger cause、split 原生回调/loading gate、保留自定义 Input/内置 Menu/子 Dropdown、焦点与文本会话清理、回调销毁/factory 异常、12 方向箭头/溢出与零尺寸箭头闲置合同。
- `popupRender(DropdownPopup)` 支持替换内容；renderer overload 通过一次性 DropdownMenuContent 保留内置 Menu。内容携带 popup weak lifetime、window 和 owner generation 检查，保存后在已销毁窗口调用会被拒绝。Default Menu 继续保留 closeOnSelect 与来源语义。
- 保留 Dropdown(props, trigger)、默认 Click、source 枚举和既有回调签名。DropdownOpenInfo 追加 cause，两字段 aggregate 初始化仍可用，结构化绑定需接收第三字段或改用具名访问。DropdownButtonProps/Slots 将完整 ButtonProps 转发给原生 Button + SpaceCompact，保留各自 activation gate。
- DropdownMetricToken::values() 末尾追加 arrow_size（6 → 7）；独立 Theme Token 驱动现有 logical TooltipArrowPath 联合填充。五份完整 golden 由现有 writer 生成，未删减其他字段；未新增 renderer ABI 或组件私有 SDL/GPU 路径。
- Gallery 新增 hover/split/flyout 和保留编辑草稿的 popupRender 实例；两个配置的 Gallery 前端 target 已构建并运行页面合同。仅更新自身 overlay 并生成目录，458 个 frozen legacy IDs 不变。
- 上游依据为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Dropdown API、实现、dropdown-button 类型/实现及 custom-dropdown/dropdown-button demo；锁定 Web Dropdown.Button 已弃用，本地 helper 保留原生组合功能。

目录 check、clang-format 22.1.3（876 文件、0 失败）、doctor healthy、strict 107/107、diff 检查通过；原始输出仅保留在忽略的 out。

尚未验证：原生 Gallery 应用/GPU、driver/system font/DPI、实际桌面/系统 IME 与 Windows/MSVC。没有运行原生窗口或操作桌面；63.1/64.*/65.1 由父线程收口，本阶段未勾选。
