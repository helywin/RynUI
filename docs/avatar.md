# Avatar 与 AvatarGroup

原生文字/图标头像参照锁定的 Ant Design 6.6.5（`4a39f54842eade4e565ab336ef6097cd7e723cdd`）。组件使用 retained 节点、现有字体 shaping、logical scene v3 与原生 Popover；普通属性更新不重新执行 slot。

```cpp
#include <ryn/rynui.hpp>

void Avatars() {
    ryn::Avatar(ryn::AvatarProps{}.text(u8"江").size(ryn::AvatarSize::Large));
    ryn::Avatar(ryn::AvatarProps{}.shape(ryn::AvatarShape::Square),
                {.icon = ryn::AvatarIcon{[] {
                     ryn::Icon(ryn::IconProps{}.name(ryn::IconName::UserOutlined));
                 }}});
    ryn::AvatarGroup(
        ryn::AvatarGroupProps{}.max(ryn::AvatarGroupMax{2}),
        {ryn::AvatarGroupItem{u8"alice", u8"A"},
         ryn::AvatarGroupItem{u8"bob", u8"B"},
         ryn::AvatarGroupItem{u8"carol", u8"C"}.icon(ryn::IconName::UserOutlined)});
}
```

`AvatarProps` 提供 `text(Prop<String>)`、`size(Prop<AvatarSizeValue>)`、`shape(Prop<AvatarShape>)`、`gap(Prop<float>)`、`visible(Prop<bool>)` 和 `layout(LayoutStyle)`。`AvatarSizeValue` 是 `AvatarSize::{Small, Medium, Large}` 或 float；静态调用可以直接 `.size(40.0F)`。数值尺寸范围为 1..1,000,000 logical dp，gap 为 0..1,000,000，非法值在修改 retained 状态前拒绝。默认 circle、medium、gap=4。三种尺寸默认 24/32/40，文字 14，图标 14/18/24；数值尺寸文字基准 18、图标为尺寸一半。

`AvatarSlots` 的 `icon` 和 `content` 都是 typed slot，各只构建一次；icon 存在时优先显示，content 可保留原生 Input、Button 等富内容。隐藏会取消输入资格并退出外部 flow，保留子树、草稿及 refs。slot 内可使用 Flex 等安排多个孩子。纯文字以实际字体测量选取不放大的可渲染字号，适配宽度和高度；字体栅格化按现有字体系统量化，最低一个 raster pixel。空间不足仍执行裁剪，零尺寸后可恢复。

头像视觉边长不超过 size，较大的 LayoutStyle 分配区域用于外部布局并将头像居中；有限宽/短高会收缩视觉边长。内容采用独立矩形 viewport。circle 使用圆内接安全矩形，square 使用避开圆角的安全矩形，保证内容不会越过形状；其可用面积比 Web 弧形 overflow mask 保守。内容与命中受 viewport、祖先及 document clip 约束；焦点等外扩装饰遵循独立 decoration clip，能够延伸至允许的祖先边界。

`AvatarGroup` 接收构建时固定的 `std::vector<AvatarGroupItem>`，最多 1024 项；key 必须唯一且非空。每项提供 reactive text、optional IconName 与 visible；统一 size/shape 可响应更新。首版 Group 接受文字和原生图标，不接受任意富 slot。每项的 inline/overflow 展示位置都保留节点，max 和显隐改变不重新构建；两处共享文字/图标属性。

`AvatarGroupMax` 使用现代 `count`、`trigger`、`placement`。count 为空表示不限制；count 范围 0..1024，0 只显示 `+N`。超过 count 时显示前 count 个可见原始头像，再追加准确 `+N`；默认 hover/top，Popover 展示实际隐藏头像。未溢出或 Group 隐藏时不显示触发器，pending hover deadline 会取消。受限宽度裁剪自然重叠队列，不自动减少 count；短高收缩头像，恢复尺寸保持节点身份。Popup 延用现有 Modal/Spin/window-layer 输入边界。

视觉只通过 `ThemeConfig::avatar.tokens` 设置。包括 text/background/group_border、三套 container_size/text_font_size/icon_font_size、三套 border_radius、border_width、group_space、group_overlapping 和 font_family；overlap 为非正数，其他尺寸非负且 container_size 正、最终字体至少 .5 logical dp。`avatar.algorithm=true` 启用组件 seed 独立派生；默认继承全局/父主题。非法最终字体、非有限值与组合 extent 在主题发布前拒绝，祖先对子孙执行原子 preflight。LayoutStyle 只控制外部布局。

首版不提供 `src`、`srcSet`、图片联网/文件加载、CORS、图片错误回调、responsive breakpoint 尺寸或 CSS/DOM 属性。仓库尚无公开 Image 管线，未提供这些接口的 stub。当前证据仅覆盖 Windows MSVC headless 的真实字体/逻辑场景与输入合同，不代表 GPU、原生窗口、Linux、系统 IME 或 AT 验收。
