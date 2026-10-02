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

Space 目前保留 H/V、wrap、typed Small/Middle/Large 和独立 main/cross gap；separator/Compact 将在后续独立收尾 change 增加。平台通用与 Windows/Linux 原生验收分别见 049 tasks/evidence。
