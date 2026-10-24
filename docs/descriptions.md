# Descriptions

`ryn::Descriptions` 是固定拓扑的多列详情面板。具体测试与验收边界见 change 081。

```cpp
#include <ryn/rynui.hpp>
#include <ryn/descriptions.hpp>

void AccountDetails() {
    ryn::Signal<int> columns{3};
    ryn::Signal<bool> shown{true};
    ryn::Descriptions(
        ryn::DescriptionsProps{}.column(columns).bordered()
            .title(ryn::DescriptionsTitle{[] { ryn::Text(u8"账户详情"); }}),
        {ryn::DescriptionsItem{
             u8"name", ryn::DescriptionsLabel{[] { ryn::Text(u8"名称"); }},
             ryn::DescriptionsContent{[] { ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿")); }}}
             .span(2).visible(shown),
         ryn::DescriptionsItem{
             u8"status", ryn::DescriptionsLabel{[] { ryn::Text(u8"状态"); }},
             ryn::DescriptionsContent{[] { ryn::Text(u8"已启用"); }}}});
}
```

`column` 默认 3；`span` 默认 1，均要求正整数。key 必须非空且唯一，声明顺序和 item 集合在挂载后固定。span 超出行尾剩余列时截断，最后不足一行时末项扩展至行尾。隐藏项不占列；恢复使用原 slot。

`labelPlacement(Horizontal/Vertical)` 控制标签位置，`bordered` 默认 false，`colon` 默认 true（bordered 不显示）。`DescriptionsSize::Default/Middle/Small` 对应锁定 Ant Design 6.6.5 的 large/medium/small。`layout(LayoutStyle)` 只控制外部布局，组件视觉通过 Theme 配置。

富内容可使用 Text、Button、Input、Select 等原生组件。普通响应式内容应在 slot 内绑定 `Prop<T>`，不要通过重建整个面板更新。没有 locale/RTL、自动响应断点、动态增删/排序、Web DOM/CSS 或 AT API；等分原生列不复制 HTML auto table。Gallery 与真实 GPU/平台验收不属于本 change。

列宽以可用宽度等分，span 合并对应列。水平 bordered 标签占一个半列单位，值占 `2 * span - 1` 个半列单位；水平 unbordered 标签最多分配 item 的一半宽度，按内在尺寸为值留下其余空间。垂直布局在每个逻辑行内分别对齐标签和值的最大高度。有限宽度参与真实文字测量与换行，短/零高度会裁剪实际绘制与命中，尺寸恢复不重挂子树。

隐藏 item 会退出 flow、取消其输入捕获与焦点、结束旧输入会话并关闭 Select popup；显式恢复后保留 Input 草稿和 slot 身份。容器不注册交互禁用祖先。slot 内的控件遵循各自的原生 Props 和 Theme。

```cpp
#include <ryn/rynui.hpp>

void DescriptionTheme() {
    ryn::ThemeConfig config;
    config.descriptions.tokens.label_background = ryn::Color::rgba8(245, 248, 252);
    config.descriptions.tokens.padding_inline = ryn::dp(20);
    config.descriptions.tokens.font_size = ryn::dp(14);
    ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[] {
        ryn::Descriptions(ryn::DescriptionsProps{}, {});
    }});
}
```

`ThemeConfig::descriptions` 支持 `seed`、`algorithm`、`tokens` 和继承。`algorithm=false` 时忽略 component seed，沿用有效父配置；启用后独立派生，不改变其他组件。colors 与 metrics 各自订阅和失效。最终长度必须有限非负，行高正，字体至少 0.5 logical dp 且可转换到文本系统使用的 logical pixel；非法更新及非法后代配置在 snapshot 发布前拒绝。普通 Theme 变化不重设编辑草稿。组件不启动动画或连续帧，更新通过正常 DirtyQueues 唤醒。
