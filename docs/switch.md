# Switch 原生 API

Switch 支持受控 `checked(Prop<bool>)` 或非受控 `defaultChecked(bool)`，两者不可同时提供；支持 disabled/loading、Middle/Small、外部 LayoutStyle。pointer 完整点击与 Space 触发切换，Enter 不切换，loading 保持已有焦点并抑制激活。

```cpp
ryn::SwitchRef reference;
ryn::Signal<bool> checked{false};
ryn::Switch(ryn::SwitchProps{}.checked(checked).ref(reference).autoFocus(true)
                .onChange([&](bool value) { checked.set(value); })
                .onClick([](bool target) {}),
            ryn::SwitchSlots{
                ryn::SwitchCheckedContent{[] { ryn::Text(u8"开"); }},
                ryn::SwitchUncheckedContent{[] { ryn::Text(u8"关"); }}});
```

checked/unchecked slots 支持 Text、Icon 和被动布局，分别挂载一次。两种内容共同确定自然宽度，状态切换保持宽度；文字 Signal 更新会重新测量。只有当前分支绘制，文字与图标限制在轨道内容预算内。交互子控件会明确拒绝并回滚 mount。

`direction(Prop<SwitchDirection>)` 的 LeftToRight/RightToLeft 镜像手柄和状态内容。SwitchRef 共享绑定，只能在创建线程调用；bound/focus/blur 返回是否成功，销毁后失效并允许复用，重复 live binding 会拒绝。autoFocus 仅挂载时执行；disabled 不可聚焦，loading 保持旧焦点合同。

有效激活先请求 onChange，再执行 onClick，两者报告同一目标布尔值。受控方不回写时展示仍服从 checked；onChange 销毁组件后 onClick 的回调副本仍可安全执行。外部 checked 更新不产生 onChange/onClick。

Switch Theme token 支持四种 inner margin（普通/Small 的 min/max）、handle_shadow、wave_spread/wave_width/wave_opacity，以及原有 track/handle 大小与 handle_background。组件算法解析该组件的主色、尺寸、fontSizeSM 和 focus；`ThemeConfig.alias.opacity_loading` 在 [0, 1] 范围内控制 disabled/loading 的轨道、手柄、内容和 spinner 透明度，默认 0.65，同时抑制手柄阴影。无效 token 明确拒绝，不发布半完成主题。

按压时手柄向轨道内部伸展30%，基础位置与逻辑两端伸长都使用 motionDurationMid（默认200ms）和 CSS ease-in-out（cubic-bezier(0.42,0,0.58,1)）。释放时收缩与移动同时过渡，快速重按或受控 checked 更新从当前呈现值衔接；LTR/RTL与Middle/Small保持一致。cancel、blur、disabled/loading 后恢复，reduced motion/motion=false直接到目标。阴影与 focus 使用共同 RoundedEffect，手柄阴影插在轨道 fill 和手柄 fill 之间，保持原有十层 quad 的 identity。`wave(Prop<bool>)` 默认启用，只有用户有效激活产生有限反馈；外部 checked 更新不会产生 wave。重复激活复用一个 range，结束或 disabled/loading/窗口失活/motion=false/reduced motion/销毁后清空效果，稳态不请求下一帧。

设计来源：[锁定 Ant Design 6.6.5 Switch](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/switch/index.tsx)、[手柄官方样式](https://github.com/ant-design/ant-design/blob/6.6.5/components/switch/style/index.ts)。官方样式使用字面量 CSS ease-in-out，区别于 Ant motionEaseInOut token。DOM/HTML/CSS、React ref 与 Web value/defaultValue 兼容别名不移植。046 的任务与证据分别记录原生功能、共同合同和各平台验收；056补充手柄按压/释放连续性回归。

共同合同已通过 Windows MSVC headless Debug/Release；Windows 真窗口 D3D12/DXIL 十次缩放运行与 200 张 readback 见 [Windows 证据](../openspec/changes/046-20261002-complete-native-switch-features/evidence/windows/README.md)。Linux 原生验收独立待完成。
