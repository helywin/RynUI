# Descriptions

`ryn::Descriptions` 是保留式多列详情面板。具体测试与验收边界见 change 081。

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

`column` 默认 3；`span` 默认 1，均要求正整数。key 必须非空且唯一，动态 `Prop<vector<DescriptionsItem>>` 按稳定 key 增删、重排，保留同 key 的 label/content 原生实例。span 超出行尾剩余列时截断，最后不足一行时末项扩展至行尾。隐藏项不占列；恢复使用原 slot。

`labelPlacement(Horizontal/Vertical)` 控制标签位置，`bordered` 默认 false，`colon` 默认 true（bordered 不显示）。`DescriptionsSize::Default/Middle/Small` 对应锁定 Ant Design 6.6.5 的 large/medium/small。`layout(LayoutStyle)` 只控制外部布局，组件视觉通过 Theme 配置。

富内容可使用 Text、Button、Input、Select 等原生组件。普通响应式内容应在 slot 内绑定 `Prop<T>`，不要通过重建整个面板更新。`rtl` 镜像实际列与 label/content 几何，`locale(DescriptionsLocale)` 提供原生 separator，标签语言由 typed label slot 声明；不复制 Web DOM/CSS、HTML auto-table 或 AT 平台桥。真实 GPU/平台验收独立记录。

Gallery 目录的 Descriptions 直接进入“项目资料”：编辑名称、选择角色、切换启用状态后保存；外部按钮调整单列/多列、名称跨列、资料显隐、横纵标签、边框及三种尺寸。`column(Prop<Responsive<int>>)` 使用共享窗口 logical width 服务自动选择列数，`breakpoints` 可覆盖断点；嵌套窄容器仍按窗口断点选择。“编辑名称”可恢复隐藏资料并调用原 InputRef，普通草稿保持。086 的 `--details-loading-only` 提供有限视口 Recording 验证，详见该 change evidence；不代表原生窗口/GPU/系统 IME/AT 验收。

列宽以可用宽度等分，span 合并对应列。水平 bordered 标签占一个半列单位，值占 `2 * span - 1` 个半列单位；水平 unbordered 标签最多分配 item 的一半宽度，按内在尺寸为值留下其余空间。垂直布局在每个逻辑行内分别对齐标签和值的最大高度。有限宽度参与真实文字测量与换行，短/零高度会裁剪实际绘制与命中，尺寸恢复不重挂子树。

label/value 的文字、内容 surface 和命中分别受各自分配区裁剪，超宽单字也不能画入相邻区域。原生 Input/Button、Switch/Checkbox/Radio（含 RadioButton）的外扩焦点装饰保留 item/祖先 hard clip 边界，不以缩小控件或重挂 slot 处理溢出；Modal 中的内容 surface 与外扩装饰也使用各自的 clip。selection 的 fill/border、wave 和命中仍使用内容 clip。

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

## 动态更新与局部工厂

`span` 接受正整数或 `Responsive<int>`，`fill()` 消耗当前行剩余列。断点所有分支的列数/span 必须正数；非法重复 key、span、column 在修改任何 cell 前拒绝，已发布布局不变。`labelRevision` / `contentRevision` 变化才替换对应同 key 工厂；标题和 extra 可用 `titleContent(Prop<DescriptionsTitle>)` / `extraContent(Prop<DescriptionsExtra>)` 显式局部替换。失败工厂退役整个 Descriptions，已挂载的新旧区域、焦点及子 popup 一并清理。

`visible` 适用于整个组件，item.visible 保留隐藏草稿。响应式集合、标题/extra 和标签/body 不依赖 VDOM；普通属性变化只更新相关 props 与几何。列等分、span 合并和真实文字换行构成原生 auto-table 替代合同。所有视觉继续来自原有 Descriptions Theme，未增加 CSS 样式入口。

```cpp
ryn::Descriptions(
    ryn::DescriptionsProps{}.column(ryn::Responsive<int>{1}.at(ryn::Breakpoint::Md, 3)).rtl(),
    {ryn::DescriptionsItem{u8"notes", ryn::DescriptionsLabel{[] { ryn::Text(u8"备注"); }},
        ryn::DescriptionsContent{[] { ryn::Input(ryn::InputProps{}); }}}.fill()});
```

107 新增 1/2/3 列窗口断点样例，保留可编辑富 cell、header 重排按钮、fill 行尾与 RTL 操作；数据标明为合成示例。

## ConfigProvider 分隔文字

未设置 `locale` 时，separator 实时继承最近 ConfigProvider 的 Descriptions 组，无 Provider 时为 `:`；显式完整 Locale（包括 `:` 和合法空串）优先。`colon(false)` 始终保留，语言不会重新启用冒号。label、content、title、extra 的普通业务 String 维持原文。

语言只更新 separator 的保留 Signal，必要文字测量沿原布局传播，不 repack 或重跑任何 slot factory，不重新仲裁输入边界。新增 `rynui.i18n_components_collections` 源码覆盖真实 Input 的反向选区/preedit/undo/focus 保留；本次未运行 build/CTest，不新增平台通过结论。
