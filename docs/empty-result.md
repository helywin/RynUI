# Empty 与 Result

静态原生空状态和结果展示，基于 retained Component、Text/Icon 与 logical CPU scene。头文件为 `<ryn/empty.hpp>`、`<ryn/result.hpp>`，也由 `<ryn/rynui.hpp>` 导出。

```cpp
ryn::Signal<ryn::String> description{ryn::String{u8"暂无数据"}};
ryn::Empty(
    ryn::EmptyProps{}.image(ryn::EmptyImage::Compact).description(description),
    {.extra = ryn::EmptyExtra{[] {
         ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"创建"); });
     }}});

ryn::Signal<ryn::ResultStatus> status{ryn::ResultStatus{ryn::Status::Success}};
ryn::Result(
    ryn::ResultProps{}.status(status).title(u8"已保存").subtitle(u8"可继续编辑或返回"),
    {.content = ryn::ResultContent{[] { ryn::Text(u8"本地结果详情"); }},
     .extra = ryn::ResultExtra{[] {
         ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"返回"); });
         ryn::Input(ryn::InputProps{}.defaultValue(u8"草稿").layout(ryn::LayoutStyle{}.width(ryn::dp(160))));
     }}});
```

`EmptyImage::Default` 是 100 dp 的 Inbox 图标，`Compact` 是 40 dp 图标和较小垂直留白，`None` 隐藏 image 区域。description 默认 `No data`，空字符串移除标签所占空间。typed `EmptySlots.image` 可替换内置图示，extra 可放置真实控件。

`ResultStatus` 复用 Status：Success/Warning/Error 使用对应静态语义，Default 表示 info，None/Processing 也静态映射 info。`ResultException::Forbidden/NotFound/ServerError` 分别表达 403/404/500，使用 Lock/FileSearch/CloudServer 图标。title/subtitle 由调用者提供。可直接传静态 Status/ResultException；响应式使用 Prop<ResultStatus>、Signal<ResultStatus> 或返回 ResultStatus 的 binding。

Result 的 typed icon、content、extra slots 可分别替换图标、提供详情和放置操作。区域顺序为 icon、title、subtitle、extra、content，与锁定 6.6.5 的结构一致。image/icon slot 中的复合布局由调用者使用 Flex/Space 组合；extra 自动居中、按 token gap 排列并在有限宽度换行；content 纵向排列。

description、title、subtitle、image、status 和 Theme 更新不重新执行普通 slot。Button interaction、Input editor、编辑值和当前输入会话保留，组件本身不添加焦点站点。销毁组件会释放子树、场景和订阅。

组件占据有限可用宽度，图示和文本块居中，文本沿现有 Text 规则换行；短宽高将实际 glyph/Icon 和原生 surface 裁剪到区域。extra 四周默认预留 4 dp，容纳原生默认 focus 装饰。极短高度按视觉顺序分配空间，后续区域可能完全不可见；调用者应提供足够高度或外部滚动容器。图标在窄区域裁剪，不提供自动缩放或自适应动态布局。

视觉设置使用 `ThemeConfig.empty.tokens` 与 `ThemeConfig.result.tokens`，包括颜色、图示尺寸、文字度量、间距和内容 padding；LayoutStyle 仅控制外部尺寸/布局。组件支持默认、Dark、Compact、父 Theme 继承、inherit=false、局部 component seed/algorithm 和 typed override。颜色与度量独立订阅；颜色不触发布局。

Empty 的 `font_size`、Result 的 `title_font_size` / `font_size` 必须为有限的固定长度，且至少为 **0.5 dp**。现有 Text 先将逻辑字号四舍五入到整数，再按显示比例栅格化；0.5 dp 对应 1 个逻辑像素，更小的正值会变成无效的零像素字体。非法值在初始 Theme 解析或响应式更新发布 snapshot 前拒绝，已有文字、glyph 和 snapshot identity 保持。该逻辑阈值不随 DPI 改变。

图示尺寸 token 允许零：内置图示休眠且不占空间，恢复正值时复用原有 Icon。小于 1 dp 的正值保留请求的布局尺寸，并用至少 1 个逻辑像素的字体生成 glyph 后裁剪；极小区域可能没有可见墨迹。自定义 image/icon slot 保留自己的外部尺寸和 margin，按含 margin 的外框居中；零尺寸 token 不隐藏自定义 slot。

参考为本地锁定 Ant Design **6.6.5**，commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Empty/Result style 与 index。遵循原生语义及主要 token 默认值；内置图示使用现有 Icon，不复刻上游 Empty/exception 大型 SVG 插画。没有图片 URL、栅格资源、网络请求、动画、状态服务、动态 slot 拓扑、DOM/CSS/Web aliases 或屏幕阅读器语义桥接。

本次通过 Windows MSVC headless 的平台通用场景与交互验证。真实 GPU、Linux 和系统 IME 没有在此 change 验证；测试中的输入会话来自现有 headless fixture，不能替代系统 IME 证据。
