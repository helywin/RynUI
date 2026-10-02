# Slider 与 RangeSlider

单值 `Slider` 与双端 `RangeSlider` 使用独立 typed Props。数值属性为 double，范围为 `SliderRange{lower, upper}`；通过 `value(Prop<T>)` 受控，或 `defaultValue(T)` 非受控，两者不能同时设置。颜色和尺寸通过 `ThemeConfig::slider` 的 Component Token 控制；`LayoutStyle` 只控制外部尺寸与位置。

```cpp
ryn::Signal<double> value{30};
ryn::Signal<ryn::SliderRange> range{ryn::SliderRange{20, 80}};
ryn::Slider(ryn::SliderProps{}.value(value)
    .onChange([&](double next) { value.set(next); })
    .layout(ryn::LayoutStyle{}.width(ryn::dp(240))));
ryn::RangeSlider(ryn::RangeSliderProps{}.value(range)
    .onChange([&](ryn::SliderRange next) { range.set(next); })
    .onChangeComplete([](ryn::SliderRange next) { /* gesture finished */ }));
```

`limits(Prop<SliderLimits>)` 原子设置 minimum / maximum / step，默认 `{0,100,1}`。要求 finite、有序、正且可表示的 step，网格数量最多 2^52；数值 clamp 到闭区间并取最近 step/mark 点，maximum 也作为可选端点。等距取较大值，范围外部输入排序，用户拖动端点不能跨越。无效输入抛 `std::invalid_argument`，组件保留原状态；如果调用者给 Signal 写入非法值，应回写合法值恢复该 Signal。

controlled Slider 仅报告候选值，显示仍由 value 决定；拖动/按键 repeat 的候选值可连续推进，无需调用者同步回写。外部 value / limits 更新不触发 onChange；limits、orientation、reverse、disabled 或 keyboard 更新取消当前 gesture，value 回写保持 gesture。

支持 primary mouse / touch 轨道点击、thumb capture 拖动和取消。范围就近选择端点，重叠/等距时沿用当前端点；thumb 按下保留抓取位置，避免跳动。成功 pointer release 完成一次；cancel、禁用、窗口失焦、焦点离开按键 gesture 或销毁不触发完成回调，已产生的非受控 change 保留。

每个 thumb 独立 Tab 焦点；方向键到下一个候选点，Home/End 到边界，PageUp/PageDown 移动十个候选点。没有 marks 时与十个 step 相同。纵向默认从下向上增加；reverse 反转位置及方向键，PageUp/PageDown 仍按数值增减。keyboard=false 阻止数值键操作，disabled 阻止全部输入并移出 Tab 顺序。数值键释放完成一次，repeat down 不重复完成。

`marks(Prop<SliderMarks>)` 使用 `{value, String label}` 列表，最多 4096 项；会排序，要求 finite、唯一、在 limits 闭区间内。`marksOnly(true)` 只选择 min/max 和 marks（对应上游 step=null 的原生模式）；默认 step 与 marks 都是候选。marks/limits 更新先验证组合，再提交；缩小 limits 前可先清除越界 marks。`dots(true)` 的完整视觉候选集合最多 4096 点，超限明确拒绝；未开启 dots 时不枚举 dense step 网格。

每个 mark 显示保留式圆点；非空 label 使用真实 Text，并在轨道下方（纵向为右方）预留空间。点击标签选择相应值并完成一次 gesture；范围模式调整最近的端点。`included(false)` 隐藏已选轨道，只把当前端点对应的 dot/label 显示为 active。动态增删 labels 保留 thumb、焦点、提示及未关联的组件，空 label 不占文字空间。

`hint(Prop<SliderHintOptions>)` 默认 Auto：thumb hover、键盘可见焦点或拖动时显示独立 Tooltip；Always 持续显示，Hidden 隐藏，disabled 优先。options 还包含 TooltipPlacement。`hintFormatter(std::function<String(double)>)` 为静态格式化回调，默认输出与 locale 无关的 double。受控模式的提示显示 value，不显示未回写的候选值；同一值不会在 idle 重复格式化。Escape 关闭提示并保留 thumb 焦点，Auto 离开后可重新显示。所有提示从初次挂载起保留，经过共同窗口浮层排序与定位。

Slider token 按锁定 [Ant Design 6.6.5 API](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.en-US.md) 与 [style 源码](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/style/index.ts) 映射 rail、track、handle、hover、active、disabled 及 dot/mark。新增 dot_size、dot_border_width、mark_gap、mark_font_size、mark_line_height 和 dot/mark 颜色；字体族/字重遵循 Typography Theme。支持 Default/Dark/Compact、嵌套继承、组件 algorithm/seed 与显式覆盖。颜色 token 只失效 material，metrics 触发布局/geometry。组件通过共同 logical quads 和 rounded focus effects 工作，不含 SDL/GPU 类型。

整段轨道拖动、多端点与端点增删仍在桌面收尾范围内，Gallery 保持 partial。实施与分平台证据见 [039 清单](../openspec/changes/039-20261002-add-single-and-range-slider/tasks.md) 和 [041 清单](../openspec/changes/041-20261002-add-slider-marks-dots-and-value-tooltips/tasks.md)。
