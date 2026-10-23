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
