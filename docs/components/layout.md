# Layout 页面框架

基线：[Ant Design 6.6.5 Layout](https://github.com/ant-design/ant-design/blob/6.6.5/components/layout/index.en-US.md)。`ryn::Layout` 自动检测直属 `Sider`，有侧栏时横向分配空间，否则纵向排列。Header、Footer 占用主题尺寸与内容高度，Content 和嵌套 Layout 分配剩余空间；多个侧栏依声明顺序排列。测量与放置使用同代缓存，窗口变化和 Prop 更新保留子组件及编辑器身份。

`ryn::Content` 已是根内容 slot 的公开类型，因此页面正文使用 `ContentArea(ContentProps, ContentBody)`。其他入口为 `Layout(LayoutProps, LayoutContent)`、`Header(HeaderProps, HeaderContent)`、`Footer(FooterProps, FooterContent)` 和 `Sider(SiderProps, SiderContent)`。`LayoutProps::hasSider` 可显式覆盖自动方向。

```cpp
Layout(LayoutProps{}.layout(LayoutStyle{}.height(dp(600))), LayoutContent{[] {
    Header(HeaderProps{}, HeaderContent{[] { Text(u8"应用"); }});
    Layout(LayoutProps{}, LayoutContent{[] {
        Sider(SiderProps{}.collapsible().breakpoint(SiderBreakpoint::Lg).collapsedWidth(dp(0)),
              SiderContent{[] { Text(u8"导航"); }});
        ContentArea(ContentProps{}, ContentBody{[] { Input(InputProps{}); }});
    }});
    Footer(FooterProps{}, FooterContent{[] { Text(u8"页尾"); }});
}});
```

Sider 的 `width` 默认 200dp，`collapsedWidth` 默认 80dp，均接受 reactive `Prop<LogicalLength>`。默认展开、Dark、不提供折叠触发器；`collapsible` 开启触发器，`defaultCollapsed` 设置非受控初值，`collapsed` 为受控值，两者不得同时设置。受控交互只发出 `onCollapse(value, SiderCollapseType)` 请求，由调用方更新 Prop。

`breakpoint` 按窗口可用逻辑宽度判断：Xs 480、Sm 576、Md 768、Lg 992、Xl 1200、Xxl 1600、Xxxl 1920；初次发布和跨阈值时调用 `onBreakpoint(broken)`，需要变化时再发出 Responsive 折叠请求，普通触发器为 ClickTrigger。重复同侧窗口变化不重复通知。

`theme(Light|Dark)` 选择侧栏配色；`reverseArrow` 支持右侧栏。`trigger(SiderTrigger{...})` 提供保留的自定义内容，`trigger(SiderTriggerMode::Hidden)` 隐藏默认与零宽触发器。折叠到 0 时保留可点击、Tab 可达、Enter/Space 可激活的外侧触发器。正文分支停止命中并结束捕获、焦点、文本会话，展开后保留草稿；隐藏触发器时由外部受控按钮展开。

Header 内容默认垂直居中；Header 高度、Header/Footer 内边距与触发器尺寸从 control metrics 派生，字体从当前 map 派生，随 Dark/Compact 算法更新。

视觉统一由 `ThemeConfig.layout.tokens` 控制，包括 Header/Footer/正文与 light/dark 侧栏/触发器背景和前景、Header 高度、Header/Footer 内边距、触发器尺寸及零宽位置、字号和焦点线。`LayoutStyle` 仅设置外部几何。非有限、负宽度、无效枚举和无效 token 拒绝提交。小窗口先保留前方固定区域，再将剩余空间非负分配给正文；不产生负数或非有限几何。

Gallery 的 Layout 独立页提供完整嵌套结构、双侧栏、响应零宽、保留草稿、受控状态、自定义/隐藏触发器和 Light/Dark 示例。DOM、CSS、SSR 和 Web 固定定位样式不是原生 API。
