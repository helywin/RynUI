# 平台通用实施证据

实际环境：Windows，`windows-msvc` / Ninja Multi-Config / MSVC x64，Debug。

## 4.3 内容范围与装饰

复核已有提交发现内容 range 与 surface ID 混用、数量变化不重映射后续 range、缺少内容范围销毁、同数量更新全量重建等缺陷。修复后，以混合 surface/content 的增长、收缩、材质更新、等值更新、销毁和 generation 重用用例验证共享实例与片段一致。

复核 Typography 发现 underline/strikethrough 未绑定、挂载末尾覆盖 code/keyboard 的实际字号、disabled/secondary 优先级及 disabled 组件 Token 订阅缺口、标题字重和间距未实际生效。均已修复并加入行为断言。

装饰片段常驻，允许挂载后从 false 改为 true。背景与圆角边框先于字形，字体度量装饰线后于字形。圆角边框使用共享 RoundedEffectStore 的 outline，避免半透明边框染色填充区域。颜色与 inline 度量采用独立 TokenIdentity。

`rynui.typography_component` 验证命令顺序、实际字体尺寸、逐 run em 度量、十行二十条装饰线、重排缩减、身份保持、滚动 translation、裁剪、纯颜色更新只修改材质，以及 display scale 1.0/1.25/1.5/2.0 的逻辑几何与标题间距。

受影响回归：`theme_runtime`、`button_scene_service`、`input_pointer`、`input_keyboard`、`input_scene_allocation`、`button_component`、`text_component`、`typography_component`、`text_component_frame` 通过。Input 交互与场景分配合同保持通过。本节为 headless/contract 证据，不代表真实 GPU 窗口验收。

## 4.4–4.7 省略与交互

省略候选仅经过 TextEngine 塑形/测量，不发布 retained glyph；保留自然形状和原始内容。逐个候选的穷举 oracle 验证非单调宽度下的最长字素前缀，覆盖 combining/CJK、零行、零宽、后缀缺字、显式换行、宽度恢复与重复查询计数。

窗口常驻 Typography 参与者拥有展开、复制、编辑和 Link 入口。复制不依赖 Input 或 clipboard `has_text()`，端口晚绑定启用入口，复制全文而非省略文本，失败不显示成功，反馈 deadline 在到期/失焦/销毁时清理。

编辑在首次挂载创建真实 Input，内部继承字体与标题行高。非活动分支跳过布局和绘制并取消 eligibility、caret 与 IME。Enter/blur 提交、Esc 先取消 IME 后取消草稿，受控拒绝保留可观测 pending draft。FocusManager 在派发之后执行队列并检查目标 generation/eligibility；场景片段归属从独立身份表验证，允许持久的隐藏分支保留资源。

`rynui.typography_interaction` 覆盖没有 Input host 的复制窗口、成功/失败/晚绑定/失焦/timer/回调销毁、展开 identity、标题编辑字体继承、IME Esc、提交/取消、受控拒绝与回写、maxLength、disabled、Link 键盘/指针/Tab、捕获取消、提交回调销毁，以及队列目标提前销毁。Windows Debug 上相关 11 项回归通过；证据仍为 headless。

## 5.1 Divider

Divider 文字节点常驻，内部 ComponentLayout 在同轮 measure/place 完成文字尺寸、轨道长度和 glyph 发布前的放置。垂直忽略并暂停标签，按当前行高取 0.9 高度与 -0.06 偏移。Theme/None/显式比例分别编码，显式比例覆盖 Theme；0 比例保留文字 padding，None 使用 no-default 的 0 padding。该规则按规格修正规划中相互冲突的说明。

`rynui.divider_component` 验证全部形式、typed slot、同帧 glyph 位置、空/非空/垂直切换、三态朝向间距、超宽文字、plain 字体、虚线实例、身份稳定、兄弟局部性、颜色只更新材质以及资源销毁。Windows `windows-msvc` Debug 通过。

## 6.1 Gallery 集成

组件参考卡片直接提供五级标题、strong/italic/code/keyboard/mark/underline/strikethrough、Link 和 Divider 样例。live samples 新增单行/多行展开、全文复制、受控正文/标题编辑、disabled Link 与 Divider 各形式；共 57 个样例，Input 增至 11 个（含两个常驻但默认暂停的编辑分支）。support overlay 与生成目录同步，Tooltip、富文本、HTML clipboard 和多行编辑仍列为缺失项。

Gallery 长跳转暴露 Input 文本层各自重建顺序的问题；窗口批处理现在覆盖 Text 与全部辅助组件，同次同步只刷新一次文本顺序。省略操作宽度测量不请求新帧，展开入口不反复切换可见性，展开时全文和操作行不重叠。`token_gallery_frame` 的滚动单次重建、结构不变与 idle 合同保持通过；目录生成/目录合同/文档合同及 TextEngine/TextScene/交互/Divider 回归通过。

## Windows 字体复核修复

真实窗口验收发现先前的 DirectWrite 枚举仍把 Segoe UI Variable 的不同 weight 描述映射到同一文件/index；FreeType 不带 variation coordinates 加载，实际绘制仍是常规字重。现在 styled 解析排除 simulated face，并跳过与该族 regular 共用文件/index 的描述，继续查找静态 Segoe UI。Windows 回归明确断言 600/700 与 italic 的真实源 face；原先允许「有 fallback note 即通过」的弱断言不能代替这些检查。实际 600 解析到 `SEGUISB.TTF`，italic 到 `SEGOEUII.TTF`；`rynui.default_font_chain` 通过。
