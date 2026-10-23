# Typography

`Title`、`Text`、`Paragraph` 支持语义颜色、真实字体、行内装饰、省略展开、复制和原地编辑；`Link` 提供指针和键盘激活。视觉来自 Typography 和 Tooltip 的 Theme/Component Token，`LayoutStyle` 只控制外部布局。

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
