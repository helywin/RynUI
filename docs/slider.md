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

`limits(Prop<SliderLimits>)` 原子设置 minimum / maximum / step，默认 `{0,100,1}`。要求 finite、有序、正且可表示的 step，网格数量最多 2^52；数值 clamp 到闭区间并取最近 step 点，maximum 也作为可选端点。等距取较大值，范围外部输入排序，用户拖动端点不能跨越。无效输入抛 `std::invalid_argument`，组件保留原状态；如果调用者给 Signal 写入非法值，应回写合法值恢复该 Signal。

controlled Slider 仅报告候选值，显示仍由 value 决定；拖动/按键 repeat 的候选值可连续推进，无需调用者同步回写。外部 value / limits 更新不触发 onChange；limits、orientation、reverse、disabled 或 keyboard 更新取消当前 gesture，value 回写保持 gesture。

支持 primary mouse / touch 轨道点击、thumb capture 拖动和取消。范围就近选择端点，重叠/等距时沿用当前端点；thumb 按下保留抓取位置，避免跳动。成功 pointer release 完成一次；cancel、禁用、窗口失焦、焦点离开按键 gesture 或销毁不触发完成回调，已产生的非受控 change 保留。

每个 thumb 独立 Tab 焦点；方向键移动一个 step，Home/End 到边界，PageUp/PageDown 移动十个 step。纵向默认从下向上增加；reverse 反转位置及方向键，PageUp/PageDown 仍按数值增减。keyboard=false 阻止数值键操作，disabled 阻止全部输入并移出 Tab 顺序。数值键释放完成一次，repeat down 不重复完成。

Slider token 按锁定 [Ant Design 6.6.5 API](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.en-US.md) 与 [style 源码](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/style/index.ts) 映射 rail、track、handle、hover、active、disabled；支持 Default/Dark/Compact、嵌套继承、组件 algorithm/seed 与显式覆盖。颜色 token 只失效 material，metrics 触发布局/geometry。组件通过共同 logical quads 和 rounded focus effects 工作，不含 SDL/GPU 类型。

本轮不含 Tooltip、marks/dots、整段轨道拖动、多端点、端点增删或动画插值。Gallery 明确标注 partial。实施与分平台证据见 [039 清单](../openspec/changes/039-20261002-add-single-and-range-slider/tasks.md)。
