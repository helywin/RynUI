# Tour

`<ryn/tour.hpp>` 提供原生 `Tour` 与 `TourTarget`。目标是带窗口所有权和组件代际的 retained 子树引用；不暴露 SDL、DOM 或业务页面偏移。

```cpp
ryn::TourTargetRef editor;
ryn::Signal<bool> open{false};
ryn::Signal<std::size_t> current{0};
ryn::TourTarget(editor, ryn::TourTargetContent{[] {
    ryn::Input(ryn::InputProps{}.defaultValue(u8"可编辑目标"));
}});
ryn::Tour(ryn::TourProps{}
    .steps(std::vector<ryn::TourStep>{
        ryn::TourStep{u8"editor", u8"编辑信息"}.target(editor)
            .description(u8"目标仍可操作，其他背景输入受遮罩限制。"),
        ryn::TourStep{u8"finish", u8"完成"}.description(u8"无目标步骤居中显示。")})
    .open(open).current(current)
    .onOpenChange([open](bool value) { open.set(value); })
    .onChange([current](std::size_t value) { current.set(value); }));
```

current 从 0 开始；省略 open/current 时使用 defaultOpen/defaultCurrent。受控导航和关闭只发送请求，应用拒绝回写时保留当前步骤与输入边界。onClose 区分 CloseButton、Escape、Finish。keyboard 控制 Escape，富编辑器和最内层 popup/Modal 保留自己的键盘处理边界。

steps 接受响应 vector，key 非空且唯一。未变 key 的富标题、描述和 cover 只挂载一次；切步隐藏保留内容，返回时编辑器身份和草稿仍在。标签属性重新连接保留 Text；替换 rich slot 或移除 key 才销毁对应页。非受控当前步骤按 key 跟随重排，移除时回退到最近合法下标；受控 current 只在显示时夹紧，不回写应用。

TourTargetRef 只能绑定一个存活的原生目标，释放后可重新绑定。定位在现有祖先顺序 translation 阶段读取真实 bounds、content clip 和 window layer。缺失、跨窗口、隐藏、无可见区域或被 Modal/外部输入限制排除的目标，均回退为居中内容；不会借目标引用提升背景输入权限。Modal 内可用的目标和精确 Tour 控件通过既有 companion 事务接入；新限制一次发布目标与控件，关闭、隐藏、失活和离页释放 leases、焦点、捕获与子浮层资格。

默认 mask 限制输入到当前目标与 Tour 控件；mask(false) 不建立这条额外限制。placement 支持 Top/Bottom/Start/End/Center，含溢出夹紧和 RTL。visible/disabled 撤销显示与输入。组件采用静态定位，无周期动画或 deadline。

省略 locale 时，previous/next/finish/close 随最近 ConfigProvider 的 Tour 组更新，无 Provider 时保持英文。显式完整 TourLocale（包括默认英文或空字符串）优先；业务步骤标题、描述和 rich slots 不自动翻译。语言切换更新保留的操作文字，保留当前步骤、已访问富内容、输入焦点和租约，不重新执行步骤 factory。

`ThemeConfig::tour.tokens` 控制 background/text/mask/highlight、width/padding/gap/radius/target_gap/highlight_width/viewport_margin 和 z_index；正文排版继承 Text。新 Theme 域追加在既有序列化域之后，原生绘制使用现有 logical scene 与共同 RoundedEffect。Gallery 的 `gallery.sample.tour.native` 含真实编辑目标、富步骤草稿、受控拒绝、子 Modal 和缺失目标。

合同参照本地锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Tour index/interface/style 与 API。107 本次新增源码尚未构建和运行；HEADLESS、原生 GPU/输入与平台验收分别由集成线程记录。DOM/CSS/React、浏览器 scrollIntoView、系统辅助技术桥接不是这些源码的验证结果。
