# Typography

`Title`、`Text`、`Paragraph` 支持语义颜色、真实字体、行内装饰、省略展开、复制和原地编辑；`Link` 提供指针和键盘激活。视觉来自 Typography 和 Tooltip 的 Theme/Component Token，`LayoutStyle` 只控制外部布局。

## 混合内容与 HTML 复制

`TypographyContent` 是拥有完整来源和只读 runs 的值；`.content(Prop<TypographyContent>)` 与原有 String 入口互斥，最后设置的入口生效。`TypographyRun` 可以组合 `type`、`strong`、`italic`、`code`、`keyboard`、`mark`、`underline`、`strikethrough`，未指定的颜色继承段落，布尔语义与段落语义叠加。`TitleProps` 使用同一入口。

```cpp
ryn::Paragraph(ryn::TypographyProps{}
    .content(ryn::TypographyContent{{
        {.content = u8"正文 "},
        {.content = u8"强调", .type = ryn::TypographyType::Danger, .strong = true},
        {.content = u8" code <>&\"", .code = true, .underline = true},
        {.content = u8"\n下一行", .mark = true},
    }})
    .ellipsis(ryn::TypographyEllipsis{.rows = 2, .expandable = true})
    .copyable(ryn::TypographyCopyable{.format = ryn::TypographyCopyFormat::Html}));
```

所有 runs 在同一个 Text scene 中使用整段来源的 UBA、Unicode 字簇边界和实际字体链，按视觉次序换行；不是独立文本组件的拼接。字簇内部的样式边界归首个 scalar 的语义所有，避免拆开组合字符与 emoji ZWJ。code/keyboard 按 Component Token 选择等宽字体、字号和盒模型，换行切分同一行内盒。主题或 reactive 内容更新保留组件身份。系统字体链缺少指定字重/斜体时沿用系统字体解析器的显式 fallback 诊断。

`TypographyCopyFormat::PlainText` 是默认值。Html 模式输出转义后的语义标签（不解析输入 HTML），保留换行、空白、方向及主题语义颜色，并同时发布未省略的原始纯文本。`TextClipboard::write_html` 默认回退 `write_text`，已有宿主无需修改；SDL 适配同时提供 `text/html`、`text/plain;charset=utf-8` 与 `text/plain`，不可变 payload 保持到 SDL cleanup，替换、失败和退出均释放所有权。锁定 SDL 的 Windows backend 不发布任意 MIME，因此私有平台适配在保留 `CF_UNICODETEXT` 的同时追加标准 `HTML Format`，UTF-8 header/fragment 字节偏移遵守 [Microsoft CF_HTML 合同](https://learn.microsoft.com/en-us/windows/win32/dataxchg/html-clipboard-format)；Windows SDK 和 User32 仅存在于平台 target。写入拒绝、容量/分配错误或错误线程不会显示成功反馈。

富文本仍通过纯文本 `TextArea` 编辑。受控 rich content 的 `onEdit(String)` 由调用方重新构造并接受 rich runs，未接受时保留待提交草稿；非受控编辑提交将内容更新为继承段落语义的单个普通 run，取消编辑保持原 rich runs。

原生能力目录排除 DOM/CSS/React 属性、任意 HTML 输入和 URL 导航。HEADLESS 与平台 mock 结果不代表系统剪贴板消费端的互操作实测；本次证据见 [106 Typography 验收](../openspec/changes/106-20261104-complete-general-and-layout-components/evidence/typography.md)。

## 提示

`TypographyCopyable::tooltip` 和 `copied_tooltip` 分别显示复制前、成功后的提示，`TypographyEditable::tooltip` 提供编辑提示。鼠标悬停或键盘聚焦入口即可查看，空字符串关闭提示，不影响操作。复制失败不会显示成功反馈。

`TypographyEllipsis::tooltip` 是显式提示文案，只在实际截断时显示；展开、足够宽、编辑或禁用时关闭。文本和操作入口各自命中，悬停复制入口只显示复制提示。配置可通过 `Prop<T>` 响应式更新，提示子树保持原有 identity。

```cpp
ryn::Paragraph(ryn::TypographyProps{}
    .content(u8"可复制和编辑的长段落")
    .ellipsis(ryn::TypographyEllipsis{.tooltip = ryn::String{u8"完整说明"}})
    .copyable(ryn::TypographyCopyable{})
    .editable(ryn::TypographyEditable{}));
```

进入编辑、入口隐藏或禁用、窗口失活与销毁会关闭对应提示。提示定位、延时、主题、箭头和窗口边界处理共用 `Tooltip`。

## 多行编辑

编辑态预建真实 `TextArea`，默认从一行自动增高，保留原文中的换行。可用 `TypographyEditable::auto_size` 设置自动尺寸和行数范围：

```cpp
ryn::Paragraph(ryn::TypographyProps{}
    .content(u8"第一行\n第二行")
    .editable(ryn::TypographyEditable{
        .max_length = 200,
        .auto_size = ryn::TextAreaAutoSize{true, 1, 6}})
    .onEdit([](ryn::String text) { /* 保存完整段落 */ }));
```

Enter 提交，Shift+Enter 换行，Ctrl+Enter 也可提交；Escape 先取消当前 IME 组合，再次按下取消草稿。失焦沿用既有提交行为。受控 `content` 未接受回写时保留待提交草稿，取消恢复原文。复制与编辑都使用原始全文，省略只影响显示。

`editable` 可绑定 `Prop<TypographyEditable>` 动态更新 `max_length` 和 `auto_size`，编辑器沿用标题字体、方向、Unicode 选区、剪贴板和输入会话。关闭自动尺寸后固定为一行高度并内部滚动，不影响保存多行文本。原生编辑保留换行与首尾空白；此行为与 Ant Design 6.6.5 Web 编辑器的删除换行/trim 处理有区别。

功能和平台验收见 [057 任务](../openspec/changes/057-20261023-complete-typography-hints-and-editor/tasks.md)。原生 Linux 与系统人工 IME 未通过本次平台通用测试获得验收。
