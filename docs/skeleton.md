# Skeleton 静态骨架

原生静态 partial：组合 `Skeleton` 与独立 `SkeletonAvatar`、`SkeletonButton`、`SkeletonInput`。参考 Ant Design 6.6.5（`4a39f54842eade4e565ab336ef6097cd7e723cdd`）`components/skeleton/{Skeleton,Paragraph,Element}.tsx`、`style/index.ts` 与 API 文档；只映射设计合同，不引入 React/CSS。

`SkeletonProps` 的 loading 默认 true；title/paragraph 默认启用，avatar 默认关闭。typed `SkeletonContent` 保留真实内容。title、paragraph、avatar 配置整体接受 `Prop<T>`，loading/round、standalone size/shape/block 也接受静态、Signal 或 Binding。

```cpp
ryn::Signal<bool> loading{true};
ryn::Skeleton(
    ryn::SkeletonProps{}.loading(loading).avatar(true),
    ryn::SkeletonContent{[] {
        ryn::Input(ryn::InputProps{}.defaultValue(u8"Retained draft"));
    }});
ryn::SkeletonButton(ryn::SkeletonButtonProps{}.shape(ryn::SkeletonButtonShape::Round));
ryn::SkeletonAvatar(ryn::SkeletonAvatarProps{}.size(ryn::ControlSize::Large));
ryn::SkeletonInput(ryn::SkeletonInputProps{}.block(true));
```

`SkeletonTitle{enabled, width}`；`SkeletonParagraph{enabled, rows, last_line_width, widths}`；`SkeletonAvatarOptions{enabled, size, shape}`。`SkeletonWidth{}` 使用默认规则，`SkeletonWidth::fraction(0.5F)` 为正文区域的一半；仅接受有限 0..1 比例，不解析 CSS string 或绝对视觉尺寸。paragraph widths 非空时按行取值；缺少/Auto 项使用上游默认满宽（超过两行时默认尾行 61%）。无 widths 时 last_line_width 只覆盖尾行。最多 1024 行，可显式 0 行。

默认无 avatar 且有 title 为 3 行，其余为 2 行；标题有 paragraph 时为 38%（有 avatar 为 50%），否则满宽。无 avatar 或无 title 时默认尾行 61%；同时有 avatar/title 的两行满宽，超过两行默认尾行 61%。avatar 默认 Large，title-only 组合默认 Square，其他默认 Circle。

布局与逻辑 quad 顺序为 avatar、title、paragraph 各行；avatar 与 body 分配互不交叉的区域。有限宽使用可用宽，无界宽使用 Theme intrinsic width；短高度逐段截断，零尺寸后可恢复。standalone 默认 Middle：avatar 1×、button 2×、input 5×控制高度；button Circle 为 1×，Round 为 capsule，Square 与上游一样保留 default 矩形比例和小圆角。block 使用有限父宽；avatar Square 零圆角。所有绝对尺寸、间距、颜色、圆角由 Skeleton Theme 控制，LayoutStyle 仅控制外布局。

首版无 active/shimmer 动画、SkeletonImage/Node、网络加载或 Web/AT 适配；没有接受却无效果的 active 属性，占位自身不注册焦点或点击目标。平台实际验收状态见 083 evidence，headless 不等于 GPU、原生窗口、Linux、系统 IME、AT 或 Gallery 已通过。
