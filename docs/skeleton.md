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

首版无 active/shimmer 动画、SkeletonImage/Node、网络加载或 Web/AT 适配；没有接受却无效果的 active 属性，占位自身不注册焦点或点击目标。平台实际验收状态见 083 evidence，headless 不等于 GPU、原生窗口、Linux、系统 IME 或 AT 已通过。

Gallery 目录的 Skeleton 直接进入“正文加载”：编辑正文、选择分享范围并发布；外部“加载/显示正文”切换骨架，“编辑正文”恢复正文并调用原 InputRef。标题、正文行数、头像和 round 可独立调整，下方独立 Avatar/Button/Input 可切换形态、尺寸与 block。加载控制始终在隐藏内容之外；加载时清理旧 popup/preedit，恢复保留普通草稿和真实控件身份。086 的 `--details-loading-only` 验证三主题和两种有限视口，实际证据及限制见该 change evidence。

loading=true 时真实 content 仅挂载一次，分支绘制/输入/焦点/popup/IME 受既有门禁管理；loading=false 恢复原组件、编辑器、普通 draft 和 refs，preedit 与旧输入会话在隐藏时清除。未提供 content 或 content slot 没有挂载任何直接子组件，且 loading=false 时，根节点退出正常布局流，因此 Flex/Space 不保留空项 gap；retained region 后续增删子组件会在布局前恢复/退出正常流。非空子组件自身的空白或隐藏语义仍由该组件负责。根节点始终可处理 loading 更新；输入门禁只归属于自身 retained content 的完整 generational ID。

content 区域沿外部 LayoutStyle 布局，并把 glyph、hit 与子控件视觉限制在分配区域。子控件自己的 editable viewport 保持其职责；focus decoration 在子控件外扩、但仍受 Skeleton 内容区域及更外层 clip 限制。需要保留完整 focus ring 时，应像普通受限布局一样给控件留出外部 margin。非 block standalone 在父 Flex 拉伸其外部占位时，内部形状仍保持 Theme 的自然尺寸；block 优先填满宽度（包括 Circle+block 组合的宽度），高度仍来自 size。

Skeleton Theme 提供 `background` 与 13 个度量覆盖：title_height、block_radius、paragraph_margin_top、paragraph_line_height、title_paragraph_gap、row_gap、avatar_gap、avatar_title_offset、control_height、control_height_small、control_height_large、control_radius、intrinsic_width。背景对应静态 `gradientFromColor/colorFillContent`，因此不暴露动画渐变末色或 deprecated 颜色别名。默认标题/行高为 controlHeight/2、行距 controlHeightXS，其他间距来自锁定上游的 padding/margin。round 以当前行高派生 capsule 半径，不新增任意视觉尺寸入口。局部 seed 仅在 skeleton.algorithm=true 时派生；所有最终度量在发布前验证有限、非负及组合 extent，非法父/后代更新整体拒绝。

普通颜色更新只标记 Material，度量更新标记 Measure/Layout/Geometry/HitTest，不重新执行真实 slot。placeholder 使用 retained logical quad；需要祖先裁剪时通过已有 logical RoundedEffect fill 限制覆盖，保持共同 renderer 上传与设备度量路径。静态 placeholder 无 timer/deadline，idle 不持续请求下一帧。
