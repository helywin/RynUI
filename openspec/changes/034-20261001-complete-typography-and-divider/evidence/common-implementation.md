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

## 8.1 全量集成验收（2026-10-02）

在 Windows `windows-msvc` Debug / MSVC x64 环境完成一次完整 CTest：**首轮 237/240 通过，180.51 秒**。保留完整原始结果 [common-full-ctest.txt](common-full-ctest.txt)，不把首轮描述为 240/240。

首轮失败项及后续结果：

| 项目 | 首轮 | 处理与补测 |
| --- | --- | --- |
| gallery_document_model | partial/planned 数量旧断言 | Divider 从 planned 提升 partial 后，应为 partial=10、planned=62、deprecated=1；同步该精确断言后通过 |
| pointer_allocation | Windows access violation | 强制重编译/重新链接原样源码后通过，10,000 次 move 仍为 0 分配；再连续 20 次通过 |
| focus_lifecycle | Windows access violation | 强制重编译/重新链接原样源码后通过；再连续 20 次通过 |

补测 **3/3 通过**：[common-retests.txt](common-retests.txt)；两项稳定性复测各 **20/20 通过**：[common-lifecycle-repeat.txt](common-lifecycle-repeat.txt)。调试用临时异常跟踪 include 已移除，测试源码与先前一致；没有跳过或放宽指针/焦点断言，也没有提交未经证实的运行时修复。

**尚未确认的边界：** Windows CrashDumps 中保存了这两个首次异常，均为 `0xc0000005`、exception address `0x0`。后续重新链接的 PDB 与首次 dump 不匹配，无法据此给出可靠的源码调用栈。重编译后未复现，当前证据不能把根因归因于源码、增量链接器或环境，也不能声称已定位并修复这两个首次崩溃。保留此记录供后续复现核查。

最终受影响的 README/架构职责/Gallery 文档/Windows evidence 合同 **4/4 通过**；Debug/Release 的 Gallery document model 重新构建通过。此前已通过的 237 项没有再次全量重跑。当前 240 项均有通过记录，首次失败及补测边界如上。

最后字体边界复核补上「全族都没有合法候选时返回 nullopt」的 guard，避免 best_index 默认 0 绕过 simulated face 排除。两个配置完整增量构建通过；字体、Gallery 模型、文档/evidence 和两项生命周期测试 **8/8 通过**。原生 Debug 默认系统缩放与 Release 无 Input 复制再次退出 0；这项边界没有改变先前已保存的系统字体解析结果与四档截图。

OpenSpec CLI **1.14.0**：`doctor --json` 为 `healthy=true`、status 空；本 change strict 通过；全仓 strict **34 passed / 0 failed**。这是当前实测，取代规划时 CLI 1.4.1 的六项既有失败统计。`git diff --check` 通过。README 中英文当前进展表已同步，未把 Linux 原生 Wayland 或物理 IME 候选窗描述为已验收。

本 change 功能实现、平台通用验证与 Windows 窗口验收已经落地；Linux 9.1–9.3 及 archive 准备 10.1 保持未勾选。未执行 push、PR 或 archive。
