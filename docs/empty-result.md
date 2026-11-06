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

父 Theme 算法变化也会在发布前验证现有继承子 scope 的 Empty/Result 字号；派生值失效时，父子快照和通知保持原状，后续合法配置可正常恢复。`inherit=false` 子树保持独立。

图示尺寸 token 允许零：内置图示休眠且不占空间，恢复正值时复用原有 Icon。小于 1 dp 的正值保留请求的布局尺寸，并用至少 1 个逻辑像素的字体生成 glyph 后裁剪；极小区域可能没有可见墨迹。自定义 image/icon slot 保留自己的外部尺寸和 margin，按含 margin 的外框居中；零尺寸 token 不隐藏自定义 slot。

参考为本地锁定 Ant Design **6.6.5**，commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Empty/Result style 与 index。遵循原生语义及主要 token 默认值。Result 的 exception 大型插画仍未覆盖；Result 的 API 不因本批 Empty 扩展而改变。DOM/CSS/Web aliases、屏幕阅读器语义桥接和浏览器 URL 自动请求不属于本批原生入口。

## 107 Empty 源码准备（尚未构建或验收）

`EmptyProps::source(Prop<ImageSource>)` 使用共同 Image，支持已解码像素、编码图片及可取消的应用 ImageLoader。预览默认关闭；图片加载、错误呈现、资源所有权及过期结果拒绝复用 Image 服务，Empty 不访问网络。原有 image slot 优先；动态内容可按区域替换。

`illustration(Prop<bool>)` 提供锁定上游 Empty 大图路径的原生 IconVector 适配，使用共同字体/图层缓存、透明度和 Theme，无 SVG runtime。默认未指定时保留原 Inbox 图标；显式 false 切回 Inbox。原生方形度量保留 artwork 的宽高比与居中留白，颜色由现有 Empty image token 与容器背景派生；不声称浏览器像素一致性。来源与 MIT license 见 [artwork](../third_party/ant-design-empty/README.md)。

新增 `EmptySlots.description` 可提供富说明。`dynamicContent(Prop<EmptyDynamicContent>)` 接收 image/description/extra 三个共享 typed slot 身份。每次更新只销毁、挂载身份变化的区域；保留相同共享指针即可保留其他区域的 Input、状态与操作。空 description 回退到响应式 description 文本，空 image 回退到原有 image/source/内置图示；空 extra 移除操作区。自定义 renderer 或通知失败会退役该 Empty，避免保留部分交互。

新增合同与 `gallery.sample.empty.dynamic` 已通过 MSVC HEADLESS Debug/Release 和 Windows hidden D3D12 的图片完成/失败/重试、富说明切换及操作区移除/恢复。结果单列于 [107 集成](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/display-windows-integration.md)，不改写 073/076 的历史数量。Empty 自身无 upstream 动画或网络状态机合同；应用通过 source 和动态内容表达状态，动画可由 slot 内实际组件提供。

073 已通过 Windows MSVC headless 的平台通用场景与交互验证，集成后的完整 Debug/Release 均为 148/148，详见 [073 集成证据](../openspec/changes/073-20261024-add-native-empty-and-result/evidence/integration-c3190b0/closure.md)。真实 GPU、Linux 和系统 IME 没有在 073 验证；测试中的输入会话来自现有 headless fixture，不能替代系统 IME 证据。

## Gallery 样例

076 在完整 token Gallery 的 Collapse/Segmented 样例之后追加以下入口，保留此前样例的顺序和标识。

| 入口 | 使用方式 |
| --- | --- |
| `gallery.empty.default` | 点击 `gallery.empty.create` 创建示例任务并更新说明；`gallery.empty.image` 在 Default 与 None 之间切换图示，隐藏时说明和操作向上收拢 |
| `gallery.empty.compact` | 使用 Compact 图示与紧凑留白展示暂无筛选结果 |
| `gallery.empty.custom` | image slot 放置原生 Search Icon，外部宽度 100 dp、左右各 50 dp margin，含 margin 的外框居中 |
| `gallery.result.controlled` | `gallery.result.status` 依次切换 Success、Warning、Error、Default/info、403、404、500 并回到 Success；`gallery.result.action` 更新继续操作反馈 |
| `gallery.result.draft` | Result content 中的真实 Input，可先编辑草稿，再操作 Empty 图示/说明、Result 状态或 Gallery Theme，内容和控件身份应保持 |
| `gallery.result.exception` / `gallery.result.custom` | 分别展示静态 404，以及由原生 Icon 和 READY Text 组成的自定义 Flex 图示 |

这些 076 样例通过现有 Theme 和 typed slots 组合组件；`LayoutStyle` 只设置外部宽度和 margin，图示使用原生 Icon。107 另加入 Empty 大型矢量插画适配与应用 ImageSource；图片下载和业务网络状态仍由应用提供。

点击目录中的 Empty 或 Result 可直接定位相应交互样例。`rynui_token_gallery_frame_tests --states-only` 通过真实目录点击、指针/键盘和文本输入，在 1200×900、560×900 有限视口检查实际 glyph、图示、操作和保留身份，也纳入完整 frame 入口。076 完整 Gallery 与八项 contracts **9/9**、文档 model/viewport、Windows D3D12/DXIL 启动 smoke 及 Astra R2 复核均已通过，见 [076 证据](../openspec/changes/076-20261024-add-empty-and-result-gallery/evidence/states-gallery.md)。人工逐组件视觉、系统 IME、屏幕阅读器与 Linux 验收独立记录。
