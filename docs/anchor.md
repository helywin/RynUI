# Anchor

`<ryn/anchor.hpp>` 提供显式原生滚动源、层级键控导航和活动项追踪。设计依据 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `Anchor.tsx`、API 与 basic/horizontal/static/targetOffset demos。

```cpp
ryn::Signal<ryn::AnchorScrollState> scroll{{0, 1600, 400}};
ryn::Anchor(ryn::AnchorProps{}
    .scroll(scroll)
    .onScrollTo([scroll](float offset) {
        // 宿主实际滚动自己的 viewport，再发布真实 offset/extent/viewport。
        auto state = scroll.get();
        state.offset = offset;
        scroll.set(state);
    })
    .items(std::vector<ryn::AnchorItem>{
        ryn::AnchorItem{u8"intro", u8"简介", 0}.children({{u8"details", u8"细节", 300}}),
        ryn::AnchorItem{u8"api", u8"API", 900}.targetOffset(24)}));
```

`scroll` 与 BackTop 的 `FloatButtonScrollState` 使用同一类型、逻辑单位和确认模式；`onScrollTo` 必须存在。`position` 是目标在滚动内容中的绝对纵坐标，须由宿主随布局更新。所有位置、范围和 viewport 为有限非负值；`targetOffset` 可为有限负值。目标请求钳制到 `[0, extent - viewport]`，尾部不足一屏时不会越界。活动项取落在 `scroll.offset + targetOffset + bounds` 内位置最大的可见可用项；未到第一项时为空。点击先发送可取消的 `AnchorClickEvent`，`preventScroll()` 可接管跳转。

`items(Prop<std::vector<AnchorItem>>)` 支持插入、移除、重排与更新；全树 key 唯一，层级深度上限为 128。复制条目保留内容身份，`richTitle` 改变时仅替换该项。每项支持响应 title/position/targetOffset/visible/disabled，父项的隐藏/禁用作用于后代。横向只展示第一层，切回纵向恢复保留内容。导航区域溢出时使用自己的 wheel 滚动，键盘焦点及活动项会显露对应条目；不重挂载条目或接管正文滚动。`activeKey` 为受控高亮，`onChange` 提交原始 key 请求；未绑定时自行追踪。普通属性不重挂载 slot。

`scrollViewport(Prop<AnchorScrollViewport>)` 可提供嵌套滚动视口的逻辑窗口 top/height；固定位置和最大可见高度随其边界变化，省略时使用宿主窗口视口。

`offsetTop` 决定固定位置及默认滚动偏移，`targetOffset` 覆盖滚动偏移，每项设置优先。`affix(false)` 保持静态布局；默认固定模式保留原布局占位并将内容固定在 viewport 顶部偏移。静态模式的活动线由 `showInkInFixed` 控制。

`duration` 默认 450ms，单调时钟驱动三次 ease-out；零时长、Theme motion 关闭或 reduced motion 立即完成。新的点击、用户滚动（宿主反馈不同于最近请求）、范围变化、禁用、隐藏、输入边界关闭、失活及销毁取消在途请求。回调可销毁组件；异常取消运动并向宿主传播。宿主应在同一次更新周期发布请求结果，拒绝请求时反馈实际状态。

视觉通过 `ThemeConfig::anchor` 控制：文字/活动/悬停/分隔轨道颜色、字体、行高、块内边距、缩进、起始内边距与焦点效果。`LayoutStyle` 只负责外部布局。Gallery 的独立页面提供真实裁剪滚动正文、滑块模拟用户滚动、富标题、层级/横向和固定/静态切换与动态条目。

不提供 URL/hash/history/DOM/CSS/React API。HEADLESS 合同不等于原生窗口、GPU、系统输入法或辅助技术平台验收；分平台结果独立记录。
