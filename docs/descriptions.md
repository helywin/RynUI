# Descriptions

`ryn::Descriptions` 是固定拓扑的多列详情面板。以下为公开 API 用法；具体实施和验证进度见 change 081。

```cpp
#include <ryn/rynui.hpp>
#include <ryn/descriptions.hpp>

void AccountDetails() {
    auto columns = ryn::state(3);
    auto shown = ryn::state(true);
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
