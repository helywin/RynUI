# Design

## Context

动机见 proposal。038 提供 source upload、能力、版本与输入上限合同；现有 WindowComponentServices 管理共同 participant、focus/pointer、LayoutEngine 和 retained surfaces。Slider 复用这些路径，正式构建仍使用 CMakePresets/Ninja Multi-Config，当前实际机器仅能验收 Windows/MSVC。

## Goals / Non-Goals

Goals：在一个 change 中完整交付基础单值/双端范围 Slider 的公开 API、主题、交互、数值与 scene 合同；后续平台 adapter 继续使用同一组件。

Non-Goals 见 proposal。组件不包含 GPU packing、SDL event 或 device 生命周期决策；外部布局不成为视觉 token 入口。

## Decisions

1. `SliderProps` 与 `RangeSliderProps` 分离，`SliderRange{lower,upper}` 保证 callback/value 类型一致。`SliderLimits{minimum,maximum,step}` 原子 reactive 更新，避免独立 min/max 信号更新到临时非法区间；不使用运行时 variant 返回两种类型。默认 limits 为 0/100/1，范围默认 0/0。有限输入且 step 网格数量 <= 2^52；允许 maximum 作为额外闭区间端点。UI 小数用 double，geometry 比例最终转换 float。
2. SliderComponentHost 由 WindowComponentServices 自动持有，类似 Typography/Divider。轨道一个非 focusable interaction，每个 thumb 有 persistent 子组件、node、surface 与独立 focusable interaction，支持标准 Tab；固定 2 个 rail/track quads 与每个 thumb 2 个圆形 quads、共同 rounded focus effect。主 ComponentLayout 放置子 node，pointer 值计算使用含 translation 的逻辑 bounds。
3. controlled 与 displayed 值分离，gesture 保存上次候选值，用于去重和 completion；不擅自写 Prop。callback 前复制必要数据，callback 后重新查 component，允许 callback 修改属性或销毁自己。范围端点不能跨越，重叠时沿当前 focus/active thumb 选择；不引入端点身份交换。
4. 公共 keys 增加 up/down/page_up/page_down，SDL adapter 只做 normalization。取消/失焦/禁用/limits/方向/keyboard 改变时清理 gesture，controlled value 回写保留 gesture；不把 pointer cancel 解释为 rollback（uncontrolled 已发出的 change 保留）。keyboard complete 只对应实际调整的键释放，重复 down 合并。
5. Slider token 按锁定 Ant Design 6.6.5 官方 [style 源码](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/style/index.ts) 和 [API](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.en-US.md) 映射本轮所需轨道/handle/active/disabled token；不复制 React/style 实现。colors 与 metrics 独立 TokenIdentity，纳入 snapshot hash/JSON/继承/组件算法，验证 Default/Dark/Compact。

## Risks / Trade-offs

- 小数网格与极端值 → 明确可表示上界、finite/overflow 校验及独立数值断言；不枚举网格。
- 重入销毁、异步 controlled 回写与取消 → gesture 生命周期与 stale ID 测试，callback 后重新获取状态。
- thumb geometry/hit order、reverse/vertical 与 scale → common tests 验证字面坐标和 Focus traversal，真实 Windows GPU/输入/resize 留证；Linux 单独验收。
- 本轮交互直接更新 geometry，未增加插值动画；Tooltip/marks 后续独立 change。Gallery 明确支持范围而非宣称上游 API 全量实现。

## Migration Plan

先提交可验证规划，再整体落地公共 API/数值/host/theme/Gallery 与合同测试；通用 Debug/Release CTest 在 Windows headless 只执行一次，保存证据并提交。Windows 原生 Debug/Release 构建、key adapter 合同与实际 Slider 窗口/DPI/resize 验收另行提交，Linux 原生独立 pending。回退仅撤销本 change 提交，不修改依赖或 renderer ABI。
