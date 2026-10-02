# Divider 原生桌面合同

```cpp
ryn::Divider(ryn::DividerProps{}
                 .variant(ryn::DividerVariant::Dotted)
                 .size(ryn::ControlSize::Small)
                 .orientation(ryn::DividerOrientation::Start)
                 .direction(ryn::DividerDirection::RightToLeft)
                 .orientationMargin(ryn::DividerOrientationMargin::length(ryn::dp(20)))
                 .content(u8"分组"));
```

`DividerText` 提供保留的 typed 文字 slot；String 内容与布局属性可绑定 `Signal`/`Binding`。Solid、Dashed、Dotted 支持水平和垂直线，圆点之间为透明间隙。`dashed(true)` 保持旧虚线行为；Dotted 优先于 legacy dashed，其他变体下 legacy dashed 优先。

Small/Middle 使用 Theme 的 `small_horizontal_margin`/`middle_horizontal_margin`；Large 和未声明 size 保留原来的有文字/无文字边距。尺寸只影响水平线，垂直线继续使用相对于当前行高的高度与行内间距。颜色、线宽、文字与边距均来自 Theme/Divider token，`LayoutStyle` 只控制外部布局。

Left/Right 为物理侧，Center 居中；Start/End 在 scoped LTR/RTL 下镜像。方向只控制标题与 rail，不改变文字 bidi 排版。

间距支持 Theme、None、Ratio（`fraction`）与 Length（`length(dp(...))`）。比例保留旧合同；长度为标题到近侧边缘的间距，近侧 rail/padding 为零，远侧保留 Theme gutter。窄容器夹紧长度和文字预算。None 保持旧的无 rail/无 padding 语义。长度必须非负有限且不能为 auto，比例必须在 0..1。

变体和颜色更新不重新测量保留文字；尺寸、文字或方位变化按需布局。圆点使用共同 logical effect，按节点 translation/opacity 和窗口裁剪绘制；每个组件最多 4096 个装饰 primitive，超限明确报错。Divider 不参与命中、焦点或 IME，销毁释放全部绘制资源。

规划与分阶段验收见 [045 tasks](../openspec/changes/045-20261002-complete-native-divider-variants/tasks.md)。原生实现和 Windows/Linux 实际窗口证据分别记录；DOM refs、CSS/HTML/React API 不移植。
