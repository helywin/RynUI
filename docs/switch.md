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

设计来源：[锁定 Ant Design 6.6.5 Switch](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/switch/index.tsx)。DOM/HTML/CSS、React ref 与 Web value/defaultValue 兼容别名不移植。046 的任务与证据分别记录原生功能、共同合同和各平台验收。
