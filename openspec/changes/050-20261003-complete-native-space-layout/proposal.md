# Proposal

## Why

既有 Space 只覆盖基本 H/V、gap、wrap 与三种对齐，仍缺少原生 separator、基线、Compact 和 Addon。用户已明确补齐已有组件的原生桌面功能；049 的真实基线和共同按角圆角 effect 可以复用，避免继续保留功能缺口。

## What Changes

- **BREAKING** 修正 Space 默认 align：horizontal Center、vertical 默认 Stretch；显式 Start 保留旧意图。增加 Baseline、LTR/RTL、typed orientation，并兼容已有 vertical。
- typed SpaceSeparator slot 与 split 别名；每个相邻 item 边界挂载一次，按 item 保持 separator 的换行、方向和 retained 生命周期。
- SpaceCompact typed Props/content：H/V、LTR/RTL、三尺寸/block、size 显式 child 优先、嵌套组合与动态销毁；连接 Button、Input/Password/Search、RadioButton 原生控件。
- 共同控件上下文控制连接角、单一 shared seam 和状态边优先，保留 hover/active/focus/disabled、影子与有限 wave；不新增 GPU ABI 或 renderer 上传入口。
- SpaceAddon 提供 typed content 与主题驱动原生附加标签，加入 Gallery、目录、文档、共同与分平台验收。

## Capabilities

### New Capabilities

- `layout-containers`：沿用 005/049 的 capability 名称，增加 Space 对齐、separator 和方向合同；main specs 当前为空。
- `control-grouping`：原生 Compact 的上下文、尺寸继承、相邻连接边、Addon 与生命周期。

### Modified Capabilities

无已归档 main capability。

## Impact

include/ryn/space.hpp、Space/Compact 组件、共同控件上下文、Button/Input/Search/RadioButton 的逻辑 CPU scene，以及 Gallery/测试/文档。保留 Core/renderer 边界，使用 049 按角 effect 和共同 SceneResources，不依赖 CSS/DOM/React。

参考锁定 Ant Design 6.6.5 Space/Item/Compact/Addon 与各控件 compact 样式。上游无 responsive size API；Web classNames/styles/component/nativeElement 不移植。风险为默认值改变、nested grouping 和重叠状态边；每阶段通过原生逻辑合同再提交。平台通用在 Windows MSVC HEADLESS D/R 完成；真实 Windows 与 Linux 验收独立记录，不以规划或 Windows 结果代替 Linux。
