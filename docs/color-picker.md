# ColorPicker

`<ryn/color_picker.hpp>` 提供原生颜色选择器。`PickerColor` 使用 typed `ColorRgb`（RGB 0..255，alpha 0..1）和 `ColorHsb`（hue 0..360，saturation/brightness/alpha 0..1），支持 Hex/RGB/HSB 转换。`parse()` 只接受完整 `#RGB/#RGBA/#RRGGBB/#RRGGBBAA`、`rgb()/rgba()`、`hsb()/hsba()`；HSB 的 S/B 必须带 `%`。空白、畸形、非有限与越界输入返回 `nullopt`，typed 构造非法值抛 `invalid_argument`。Hex 输出按八位通道舍入，RGB/HSB 文本使用可往返的 double 最短十进制表示；灰度/黑色保留 HSB hue；有彩色的 360 度 hue 与 0 度规范化为同一颜色。

```cpp
ryn::Signal<ryn::ColorPickerValue> value{ryn::ColorPickerValue{ryn::PickerColor{ryn::ColorHsb{215, .8, .9, .75}}}};
ryn::ColorPicker(ryn::ColorPickerProps{}
    .value(value).showText(true).allowClear(true)
    .onChange([value](ryn::ColorPickerValue next, ryn::String) { value.set(next); }));
```

`ColorPickerValue{}` 是 clear/空值；实色使用 `PickerColor`。渐变由 2..64 个 `ColorStop{key,color,percent}` 构造，key 非空且唯一，percent 有限且在 0..100。构造稳定排序，同位置 stops 保持顺序；`sample()` 以 RGB/alpha 线性插值，边界使用最近颜色，重合 stop 在位置处使用右侧色。透明度的棋盘背景仅用于预览，不会写入色值。

面板有真实 SV 二维区域、hue 与 alpha 色带、四通道 InputNumber、完整格式文本 Input，以及 HEX/RGB/HSB 切换、clear、模式和渐变 stop 编辑。色标可指针选择/拖动、添加/移除、编辑百分比；至少保留两个。色带支持方向键/Home/End；SV 的左右键调整饱和度，上下键调整亮度。`.direction()` 提供 RTL 的色带/指针/键盘映射。输入和浮层继承既有主题、焦点与 IME 合同。

`value/defaultValue`、`format/defaultFormat`、`open/defaultOpen` 分别互斥。受控属性只发送请求，调用方回写接受；拒绝后保留权威值；手势中回写同候选保持拖动，写入不同权威值则取消当前手势和捕获。指针和键盘连续交互发送 `onChange(value,css)`（css 使用有效 RGB(A)/linear-gradient 表示，独立于面板的 HSB 格式），结束时发送一次 `onChangeComplete(candidate)`；取消、隐藏和移除不发送完成。离散编辑与预设在变化时各发送 change/complete，clear 另发送 `onClear`。范围和格式更新不虚构用户事件。

`modes()` 指定允许的 Single/Gradient 模式，`mode(Gradient)` 初始化渐变。预设组有稳定 key、label、colors 与 default_open，支持动态新增/更新/删除。模式往返切换缓存未编辑的渐变；离开渐变时使用第一个色标。`disabledAlpha` 禁止透明度输入，并在用户修改时将候选实色或各渐变 stop 的 alpha 归一为 1（外部受控值不擅自改写），`disabledFormat` 固定当前格式。disabled、size、status、showText、trigger、placement、arrow 和 autoAdjustOverflow 复用 Button/Input/Popover 合同。

`ColorPickerSlots` 支持保留 trigger、text_render、panel_render 和 extra。`text_render` 接收 typed `ColorPickerTextContext`，其 value/text/format 均为响应属性，可将 text 直接交给 `TextProps::content`。`panel_render` 接收 typed `ColorPickerPanelParts`，可通过公开 `renderPicker()/renderPresets()` 各挂载一次 picker 与 presets，组合自定义布局和富子控件；普通色值更新不重新执行这些 slots。浮层关闭、禁用、窗口失活、输入边界变化及卸载取消捕获/焦点/编辑会话。

`ThemeConfig::color_picker.tokens` 提供 panel_width、saturation_height、rail_height、gap、thumb_size 与 checker_light/checker_dark/thumb/focus。颜色与尺寸独立失效，支持继承、Dark/Compact 和 component algorithm。面板本身没有持续动画或 idle 帧；Popover/Button 的 reduced motion 策略继续适用。LayoutStyle 只控制外部布局。

颜色场由共同 `graphics::sampled_color_field` 生成 logical CPU scene v3 quads，经 RetainedSurfaceService 和既有 SceneResources 事务上传；不新增 shader、GPU ABI、SDL 路径或第三方依赖。色带使用有限采样，精确值独立于视觉采样。

参考固定为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 color-picker/interface.ts、ColorPicker.tsx、渐变和 panel-render demos。Web DOM/CSS/portal 接口不移植。Linux HEADLESS 结果见 change 107 的 [ColorPicker 验证](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/color-picker-linux.md)，Linux 私有 Wayland/Vulkan 的有限控件输入、系统字体、input-area/cursor 与离页清理见 [主分支组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/color-number-integration.md)；系统 IME 候选 UI、跨显示器/DPI、AT 和 Windows 仍未验证。Gallery 同时显示实际解析的九个组件 Token 字段。
