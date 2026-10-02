# Flex 与 Space 原生布局

Flex 使用 typed Props/content 与 reactive Prop；LayoutStyle 只控制外部 grow/shrink/basis/order/尺寸，不改变组件视觉 token。参考 Ant Design 6.6.5 的原生布局含义，Web 的 CSS 字符串、DOM/component/React 属性不移植；上游无 responsive API。

```cpp
ryn::Signal<ryn::FlexWrap> wrap{ryn::FlexWrap::WrapReverse};
ryn::Signal<ryn::FlexDirection> direction{ryn::FlexDirection::RightToLeft};
ryn::Flex(ryn::FlexProps{}.wrap(wrap).direction(direction).gap(ryn::dp(8), ryn::dp(12)),
          ryn::FlexContent{[] {
              ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"一"); });
              ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"二"); });
          }});
```

`wrap(bool/Prop<bool>)` 继续映射 NoWrap/Wrap；最后一次 wrap 配置决定实际订阅。WrapReverse 使用同一 greedy line breaks，从相反 cross 边堆叠。Horizontal RTL 反转主轴坐标，Vertical RTL 反转 cross 坐标；与 WrapReverse 组合时 cross 反转取异或。它们保留声明、scene paint 与键盘顺序，不重新执行 content。

Start/End 为当前 flow 起止，Center/SpaceBetween/SpaceAround/SpaceEvenly 保持现有空间分配。Left/Right justify 只在 horizontal 固定到物理左/右，vertical 按 Start 回退。FlexStart/FlexEnd/Normal/Stretch justify 为相应 Start/End 原生等价值；align Normal 对应 Stretch，FlexStart/FlexEnd/SelfStart/SelfEnd 对应现有 flow Start/End。RynUI 使用 horizontal 文字书写，不提供 CSS 字符串或百分比解释器。

| 上游 flex 原生含义 | RynUI 外部布局入口 |
| --- | --- |
| `flex: 1` | `LayoutStyle{}.flex_grow(1).flex_shrink(1).flex_basis(dp(0))` |
| auto | `.flex_grow(1).flex_shrink(1).flex_basis(auto_length)` |
| none | `.flex_grow(0).flex_shrink(0).flex_basis(auto_length)` |
| grow/shrink/basis 三值 | 三个对应 typed reactive 字段 |

grow/shrink 为有限非负值；basis 为非负 logical length 或 auto。min/max 控制弹性分配冻结，order 只改变视觉布局顺序。所有值由现有 LayoutEngine 分配，组件不添加私有 layout parser。

## 基线与默认对齐

**BREAKING：049 起 `FlexProps` 默认 align 为 Stretch。** 依赖旧顶部/左边对齐的调用必须显式 `.align(FlexAlign::Start)`。自动 cross 尺寸拉伸，显式尺寸保持，min/max 与 margin 继续生效；内部控件在实际拉伸尺寸上重新测量，避免把旧高度的文字基线传播给外层。

`.align(FlexAlign::Baseline)` 对齐一行内 Text、Typography、Button、Input 的真实第一行文字基线；`.align_self(FlexAlignSelf::baseline)` 允许单项参与。基线来自系统字体测量，并与 intrinsic size 同缓存。Box、嵌套 Flex 和控件传播 padding、margin、居中偏移；无文字项使用下边缘合成基线，图标或 spacer 不遮蔽后续文字标签。每行分别保留最大 ascent/descent，WrapReverse 对齐相同物理文字基线。vertical Baseline 在横向书写中回退 cross Start。

基线切换、字体、margin 或内部对齐影响已参与基线的祖先时刷新测量；纯方向/justify 更新复用测量，仅更新 placement、glyph geometry 与命中区域，保留 content/scene/interaction 身份。销毁取消 reactive 订阅；没有布局动画或持续 deadline。

## Space 对齐与方向

**BREAKING：050 起 `SpaceProps` 默认 align 为 Auto：horizontal Center、vertical Stretch。** 依赖旧顶部/左边对齐的调用必须显式 `.align(SpaceAlign::Start)`。Baseline 与 Flex 使用相同真实字体基线合同；Space 按内容顺序和 gap 排列，不读取子项的 flex grow/shrink/basis/order。

`orientation(SpaceOrientation::Horizontal/Vertical)` 与旧 `vertical(bool/Prop<bool>)` 的最后配置决定方向及 reactive 订阅。`direction(FlexDirection::LeftToRight/RightToLeft)` 保留声明、绘制与键盘顺序，纯 RTL 更新复用测量。Space 保留 wrap、typed Small/Middle/Large 和独立 main/cross gap。

`.separator(SpaceSeparator{[] { Text(u8"/"); }})` 在每两个直接内容项之间挂载一个独立分隔分支；`.split(...)` 为同一字段别名，最后设置优先。空/单项不调用分隔 slot，分隔支持多个富内容根项并继承 Space 的 Theme。共同布局、绘制与键盘顺序交错；wrap 可把分隔单独换行，RTL 只改位置。方向、gap、Theme 更新保留已挂载内容；删除主项清理多余或首项前的分隔，slot 抛错由 Host 回滚。

## SpaceCompact

`SpaceCompact(SpaceCompactProps{}.size(ControlSize::Small).block(true), SpaceCompactContent{...})` 提供无 gap 的控件组合。typed orientation 支持 H/V，direction 支持 RTL；block 占满可用 inline 宽度，vertical 自动宽度项拉伸到组宽，显式宽度与 min/max 保留。共有边按相邻控件实际 border width 的较小值重叠；无边框项不制造边框。

未显式调用 size 的控件继承最近 Compact；显式 Middle 也优先于组尺寸。嵌套组继承最近尺寸并交叉保留外角，空嵌套组不占首尾位置。方向/尺寸变化与删除项更新 retained 布局及四角 geometry，不重新执行内容。

Button 六视觉变体已接入：共有边按 hover、focus/active、normal、disabled 优先，稳定 paint/Tab 顺序；嵌套组在外层共有边传递实际子控件状态。按角 fill、shadow、focus、wave 与虚线使用共同 logical effects；loading spinner 保持在背景之后，有限动画结束后无持续 deadline。每组支持最多 1024 个已接入控件，seam/虚线各自上限 4096 effects，超限显式拒绝并由挂载回滚清理。

Input/Password/Search/RadioButton 与 SpaceAddon 按 050 的下一阶段接入。平台通用与 Windows/Linux 原生验收分别见 049、050 的 tasks/evidence。
