# Proposal

## Why

RynUI 已有 Tooltip、Popconfirm 和选择器弹层，但缺少承载真实编辑器和操作按钮的通用气泡卡片。新增原生 Popover，让用户可以在触发入口附近编辑、选择和执行操作，同时保留内容身份与草稿。

## What Changes

- 新增 typed PopoverProps、PopoverTrigger、PopoverTitle、PopoverContent 与 PopoverSlots，提供字符串 title/content 便捷入口。
- 实现受控/非受控开闭、disabled、hover/focus/click 组合、进出延时、12 方位、箭头与 overflow 调整。
- 复用现有定位和 retained scene 路径；富内容保留原生输入、键盘激活、子 popup、焦点与 wheel 归属。
- 新增独立 Popover Theme 完整链、API/model/component/Theme/组合测试、docs/popover.md 与编译示例。
- 首版排除 Web 兼容属性、contextMenu、动画、RTL、内部滚动容器与 OS 可访问性桥接；不新增依赖，不修改 Gallery、全局完成记录或 renderer ABI。

## Capabilities

### New Capabilities

- `native-popover`：保留式交互富内容气泡、输入边界与独立主题合同。

### Modified Capabilities

无。

## Impact

影响公开组件头文件、WindowComponentServices、组件实现、Theme snapshot/runtime、CMake 测试注册与五份 typed golden。设计核对本地锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`。风险集中在同步卸载回调、受控关闭拒绝、嵌套浮层路由和受限视口裁剪，通过 MSVC headless Debug/Release 针对性测试验证；不据此宣称 native/GPU/Linux/IME/AT 已验收。
