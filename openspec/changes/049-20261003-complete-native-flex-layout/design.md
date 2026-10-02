# Design

## Context

见 proposal.md。已有 005 layout-containers 的 H/V、bool wrap、grow/shrink/basis/order 与局部 placement 合同继续适用。源码缺少 baseline；TextMeasurement 已有 first_baseline，可以复用实际字体测量。当前 main specs 为空，沿用该 capability 名称新增 delta。

2026-10-03 锁定检查 [Ant Design 6.6.5 interface](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/flex/interface.ts)、[utils](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/flex/utils.ts)、[style](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/flex/style/index.ts) 和 [W3C baseline 规则](https://www.w3.org/TR/css-flexbox-1/#align-items-property)。上游无 responsive API；React/CSS/DOM/component 元素入口不移植。

## Goals / Non-Goals

Goals：补齐原生 Flex 布局缺口与默认值，复用现有弹性数值，不新增样式解释器。字体基线在 Core logical 测量缓存中保留。

Non-Goals：CSS 字符串/百分比解析、垂直书写、DOM 元素/React props；公共 Theme direction/disabled/size/locale 与 Space 的 separator/Compact 有各自后续范围。

## Decisions

1. public FlexWrap enum 与 bool 重载并存，用可选 typed Prop 记录最后一次配置，避免把 Signal<bool> 转换为临时派生 Signal。internal wrap_reverse 新值追加，三值合法性在发布模型前验证。
2. public FlexDirection 表示 LTR/RTL，内部 FlexLayout 的 `right_to_left` 标志追加，不重排声明或 interaction registry。横向主轴镜像、纵向 cross 镜像，wrap_reverse 异或 cross 方向。物理 Left/Right 在横向明确转换为物理边；Normal/Stretch justify 等价 Start，align Normal/SelfStart/SelfEnd/FlexStart/FlexEnd 采用有文档的 typed 映射。
3. intrinsic measurement 结果增加可选 first baseline，保留旧 Size 返回 lambda 的隐式转换；缓存同时保存 size/baseline。Text/Typography 提供实际 first_baseline（含 insets），Input 编辑 viewport 提供自身文字基线。非文字叶无 explicit baseline，在参与 baseline align 时合成下边缘。
4. Box、共同 HorizontalContent/InputContent、Flex 容器按首个可用文字 baseline 传播内部 padding、居中和 margin 偏移；控件 spacer/icon 无文字时不遮蔽真实标签。其它 ComponentLayout 无文字时使用边缘回退。horizontal line 记录 ascent/descent；align-self Baseline 与容器 Baseline 共同参与，行高取最大上方加最大下方范围。当前横向书写下 vertical Baseline 按 cross Start 回退。
5. baseline align 转入/转出可能改变 line 高度，采用 Measure/Layout invalidation；其它 align/justify/direction 保留 placement。子内容与字体测量修订触发 baseline 缓存刷新。line scratch 保留容量，无每帧无界结构或新 GPU primitive。
6. default Stretch 是对基线的明确修正，旧需 Start 的示例/测试显式设置，回归按真实意图修复，不以恢复错误默认绕过验收。grow/shrink/basis/order 的完整 native 映射留在 LayoutStyle，避免 duplicate Flex 私有外部布局入口。
7. 平台通用以 Windows MSVC Ninja Multi-Config `windows-msvc-headless` Debug/Release 完成；补入缺失的 Layout/Flex 公共 fixture，完整 CTest 验证 Core 边界。`windows-msvc` D/R Gallery frame 与真实窗口验证系统及 1/1.25/1.5/2 render scale 的基线、wrap/RTL、resize/命中/idle。

## Risks / Trade-offs

- [默认 Stretch 改变旧布局] → 标明 BREAKING，按使用意图显式 Start 并完整回归。
- [缓存 baseline 丢失或 margin 重复] → 同缓存 size/baseline，实际混合字号/多行/控件与上下 margin 合同覆盖。
- [反向轴改变键盘顺序] → 仅坐标映射，不修改声明、scene fragment 或 focus 排序。
- [baseline 跨祖先测量] → 仅已测 child 值传播，不重新执行 Component 或增量改变字体 shape。
- [默认栈与窗口裁剪] → D/R 实际 Gallery smoke 和零约束/窄窗口回归。
