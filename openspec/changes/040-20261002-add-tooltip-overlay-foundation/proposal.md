# Proposal

## Why

Slider、Typography 的文字提示以及后续弹出组件缺少独立窗口浮层。用户要求推进 Tooltip、Slider marks/dots，并收尾已有组件；本 change 先建立它们共用的浮层与完整基础文字提示合同。

## What Changes

- 增加 typed TooltipProps / TooltipTrigger slot：reactive title/open/disabled/placement、受控及非受控、hover/focus 延迟、Escape、箭头、十二种 placement、边缘翻转与移位。
- 增加窗口浮层绘制层、锚点定位与共同输入观察；保持组件 lifetime/theme 归属，不参与父布局尺寸，不覆盖 child handlers。
- 增加 Tooltip Component Token、Default/Dark/Compact、继承与覆盖；颜色、几何及文字分别失效，使用共同 logical scene。
- 增加 Gallery 示例、合同测试、Windows 原生验收；修复已有 Gallery 支持状态数量回归，并记录已有组件收尾清单。

非目标：DOM/CSS/HTML、交互式 Popover、OS 顶层窗口、系统 accessibility bridge；Slider marks/dots 由紧接的独立 change 实施，不把规划写成已实现。已有组件的其余收尾按明确功能缺口逐项推进。

## Capabilities

### New Capabilities

- `tooltip-overlays`：文字提示的可见性、输入、定位、主题和窗口浮层生命周期。

### Modified Capabilities

无；当前主 specs inventory 为空。

## Impact

公开 Tooltip header、ComponentHost paint traversal、WindowComponentServices 同步阶段、共同 Focus/Interaction、Theme、Gallery 和 tests。无新增依赖，无 renderer ABI 或 SDL 组件路径。主要风险为 overlay paint order、焦点/hover 生命周期、回调重入与小窗口定位，使用确定性负例和 Windows 实窗验证。
