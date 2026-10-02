# Proposal

## Why

039 的 Slider/RangeSlider 已有基本输入，但缺少刻度标签、离散选择和数值提示，无法完整表达带标记的桌面数值范围。040 的窗口提示层已可复用，按用户同时选择 Tooltip 与 Slider、并要求完成已有原生组件的授权继续实施。

## What Changes

- 新增 typed reactive marks、dots、included 与 marks-only 离散模式，标签使用真实 Text 与 Theme。
- 鼠标、键盘、外部受控值共同使用 step/marks 候选集合；标签点击选择对应值，非法配置原子拒绝。
- thumb 使用 persistent Tooltip 显示数值，提供 Auto/Always/Hidden 与 formatter/placement；拖动、键盘焦点和 hover 自动显示。
- 更新 Gallery、文档和合同，保存平台通用及 Windows 原生 evidence。

本 change 完成标记和提示这一可独立验证阶段；整段 track 拖动、动态多端点在紧接的 Slider 收尾 change 继续，不把本阶段称为完整 Slider。Web 专用 API 不移植。

## Capabilities

### New Capabilities

- `slider-marks-and-hints`：标记/离散点、选中轨道语义与数值提示。

### Modified Capabilities

无。主 specs 目录尚无已同步能力；039 和 040 的现有增量合同继续有效。

## Impact

影响 Slider 公开 props、共同值计算、内部持久 Text/Tooltip 组合、Slider Theme token、Gallery 和测试。不新增依赖、GPU ABI、SDL 组件路径。主要风险为 marks/limits 动态一致性、标签扩大布局与命中范围、受控提示/输入回调重入；使用数值负例、生命周期、失效/idle 合同与实际 D3D12 验收。Linux 原生验收需实际 Linux 桌面，独立 pending。
