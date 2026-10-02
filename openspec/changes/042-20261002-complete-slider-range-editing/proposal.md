# Proposal

## Why

041 已完成刻度、点与值提示；现有 Slider 的剩余桌面功能为整段轨道拖动、多端点编辑与逐端点禁用。用户要求将已有 partial 组件做完，本 change 补齐 Slider 原生功能并保留现有单值/双端 API。

## What Changes

- 新增 typed `MultiSlider` / `MultiSliderProps` / `SliderValues`，支持 0–64 个端点、reactive 值/受控候选、独立 Tab 焦点和 Tooltip；沿用 limits/marks/dots/included/主题。
- 双端和多端支持 draggableTrack；普通轨道点击继续就近移动端点。整段拖动使用抓取快照和单一偏移，保持规则 step 网格的间距；不规则 marks 仍逐点归一化。
- MultiSlider 提供 editable、minCount/maxCount：空轨道点击插入，Delete/Backspace 或跨轴拖出 130 logical px 删除，成功释放完成一次；cancel 不伪造完成。editable 与 draggableTrack 互斥，marksOnly 与 draggableTrack 互斥。
- 逐端点 disabled 和全局 disabled；含禁用端点时停止编辑与整段拖动，轨道只选择允许的端点。
- 增加原生 focus/blur ref、首次挂载 autoFocus；hint overflow 配置与纵向默认位置。生命周期与动态配置保留未变化端点和 sibling。
- 更新 Gallery、公开文档与验证证据；实现完成后将 Slider 的原生支持标为 implemented，Web DOM/CSS/ReactNode API 继续排除。

## Capabilities

### New Capabilities
- `slider-range-editing`: 桌面多端数值输入、轨道整体移动、端点编辑、逐端点禁用和原生焦点控制。

### Modified Capabilities
无。当前 `openspec/specs` 尚无已归档 capability；039/041 的既有数值/提示合同保持兼容。

## Impact

涉及 `include/ryn/slider.hpp`、Slider host/value helpers、公开 umbrella、Gallery 与测试。固定数组改为保留端点记录，使用 041 的 append_slot 挂载可变子树；不修改 renderer ABI，不新增第三方依赖。Windows/MSVC headless 验证平台通用合同，Windows native 验证窗口/GPU/输入/DPI；Linux native 独立待验。

基线：[Ant Design 6.6.5 API](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.en-US.md) 与其 `@rc-component/slider ~1.1.1` 的发布源码。风险为动态 count 与受控 echo 的一致性、焦点/捕获撤销和重入销毁；用明确上限、原子校验、generation IDs 和运行测试验证。非目标为 React/DOM/HTML 兼容、Web styles 与未建立的系统 accessibility bridge。
