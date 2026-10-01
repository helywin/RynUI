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
