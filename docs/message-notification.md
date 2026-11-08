# Message 与 Notification

在窗口 content 中挂载 holder，用 Ref 从按钮等事件中打开提示。Ref 可按值捕获；holder 卸载后，命令返回 false。每个 Ref 同时只能绑定一个 holder；可以在旧 holder 卸载后重绑。所有操作在所属 UI 线程进行。

```cpp
#include <ryn/rynui.hpp>

void FeedbackControls() {
    ryn::MessageRef messages;
    ryn::NotificationRef notifications;
    ryn::MessageHolder(ryn::MessageHolderProps{}.ref(messages));
    ryn::NotificationHolder(ryn::NotificationHolderProps{}.ref(notifications));
    ryn::Button(ryn::ButtonProps{}.onClick([messages, notifications] {
        static_cast<void>(messages.open(
            u8"save", ryn::MessageProps{}.content(u8"Saved").status(ryn::NoticeStatus::Success)));
        static_cast<void>(notifications.open(
            u8"job", ryn::NotificationProps{}.title(u8"Export").description(u8"Ready").duration(0),
            {.actions = ryn::NotificationActions{[notifications] {
                 ryn::Button(ryn::ButtonProps{}.onClick([notifications] {
                     static_cast<void>(notifications.close(u8"job"));
                 }), [] { ryn::Text(u8"Dismiss"); });
             }}}));
    }), [] { ryn::Text(u8"Save"); });
}
```

在已有 Window content slot 中调用 `FeedbackControls()`。holder 归该 Window 与其逻辑祖先所有，多个窗口的 Ref 不共享状态。`open` 需要非空 key，返回值表示该次命令结束时对应 entry 仍存活；同步回调卸载/替换时可能返回 false。`close(key)` 返回是否关闭了该 entry；`clear()` 返回 holder 是否仍可接收该命令。

Message 支持 `content(Prop<String>)`、`status(Prop<NoticeStatus>)`、`duration(Prop<double>)`、`onClose`，富内容为 `MessageSlots::content` / `MessageContent`。Notification 支持 `title`、`description`、`status`、`placement`、`duration`、`closable`、`onClose`；富内容为 `NotificationTitle`、`NotificationDescription`、`NotificationActions`。placement 包含 TopLeft、TopRight、BottomLeft、BottomRight。所有状态 Success、Info、Warning、Error、Loading 均有实际图标和 Theme 颜色；默认 Loading 图标接共同 `Icon.spin`，切回普通状态停止。富 slots 内的自定义 Icon 继续由应用控制；当前 Notice API 没有独立 icon slot。

同 key `open` 只更新明确提供的字段，不改变 entry 的插入顺序。未提供的属性连接和 slots 原样保留。传入新的 typed slot 会只替换该区域；传入空 slot 函数可清空该富内容区域。字符串更新不会移除已有富内容；更新富内容里的 Text 请把 Signal 绑定到该 Text。其他 entry、未替换区域中的 Input 编辑器、草稿与 ref 不重挂。原有 slot 的 Theme scope 继承 holder；颜色、尺寸和位置变化沿现有 reactive 更新链处理。

holder 的 `maxCount(Prop<std::size_t>)` 默认为 8，允许 1..1024；超限关闭最早插入的条目，包括 duration=0 条目。reactive 减小 maxCount 使用相同策略。close、clear、到期和淘汰会在销毁后调用对应 onClose 一次；holder/window 卸载只清理资源，不调用 onClose。close/clear/到期批次使用完整 entry generation，回调新建的同 key 条目不会被旧批次删除。回调可同步卸载、更新其他条目或重绑已卸载的 Ref。挂载中的 slot 遵守 Runtime 的既有约束，不能重入挂载；可同步卸载自身。slot 抛异常时保留未替换的旧 region；107 对失败的属性更新退役该 entry，避免保留部分更换的属性连接。

maxCount 淘汰在每次关闭回调后重新读取当前上限和存活条目数，满足上限就停止；回调修改上限、关闭其他条目或重绑 holder 时不继续使用旧删除数量。普通 notice 窗口层的固定 surface 和 content-range 填充都使用描述区域的实际裁剪；裁剪不压缩真实子控件尺寸，并保留外部 clip 内有效的焦点装饰。

最后一条 notice 关闭后，窗口中独立打开的 popup 仍正常响应自身的 value、颜色和位置变化，不保留由 notice 显示期间产生的旧填充缓存。

duration 单位为秒，Message 默认 3、Notification 默认 4.5；0 持久，正数到期关闭。允许有限非负值，最大十年。每次同 key open 都按更新后的当前 duration 重新计时；reactive duration 改变也重新计时。使用窗口的非阻塞单调 tick/deadline，过期时间不因时钟倒退而提前。窗口失活继续计算真实到期时间。无 entry 或只有 duration=0 的静态内容时，notice 不安排持续帧。

107 增补（[Windows 阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)）：Message/Notification 均支持 `pauseOnHover(Prop<bool>)`（默认 true）、`showProgress(Prop<bool>)`（默认 false）与 `motion(Prop<bool>)`（默认 false）。悬停保存剩余时间，移出从剩余时间继续；进度条来自同一计时器。motion 使用 Theme fast 时长与共同 paint opacity，结束、隐藏、失活和 reduced motion 清理动画 deadline；退出期间立即屏蔽输入。clear/maxCount 为立即销毁以维持原有重入合同。Loading 旋转独立于 entry 的进出 motion 和 expiry；hover 暂停到期不停止旋转，折叠/隐藏/离页/窗口失活/Theme 或系统 reduced motion 沿共同 Icon 合同停帧及恢复。

`openAsync(key, props, slots)` 返回 `NoticeHandle`，`closed()` 是 `shared_future<NoticeCloseResult>`，可非阻塞 `wait_for(0)` 或交给应用异步等待；不得在 UI 线程阻塞等待。Closed 表示实际关闭，Cancelled 表示 holder/window 销毁或未绑定，Failed 表示属性应用失败。`handle.close()` 绑定精确 entry 与 holder generation，不能关闭同 key 的后来新条目；同一存活 entry 的多个句柄共同等待其关闭。future 可以跨线程观察，UI 命令仍只在 holder 所属线程执行。应用可将 Ref 注入任意业务服务；[App 范围服务](app.md) 已使用真实 holders 与捕获上下文，不引入隐式进程级窗口单例。

holder 的 `stack(NoticeStack{true, 3})` 启用按 placement 分组的折叠栈。超过阈值只呈现最新正文和若干保留条带；旧正文与 action 暂停交互，悬停展开，移出折叠，调整 stack 不重建富内容。

Message 顶部居中；Notification 四角向内堆叠，标题、描述、真实关闭 Button 和 action 区域分开保留。宽高约束到当前窗口，超长内容裁剪；内容内的 TextArea/Select 等保留已有输入与滚轮功能，首版没有额外通知滚动容器。open 不请求焦点；关闭 focused 内容安全结束输入会话并清理焦点。Modal、Spin 与隐藏祖先的输入边界仍生效。notice 仅在子控件处理后消费自己实际顶层命中的滚轮回退，不处理 Escape，不吞被覆盖 sibling 或子 popup 的事件。

`ThemeConfig::message` / `notification` 分别支持 `tokens`、`seed`、`algorithm`。颜色为 background/text/description/success/info/warning/error/loading；指标为 width、padding_block、padding_inline、gap、stack_gap、offset、border_radius、font_size、line_height、title_font_size、title_line_height、icon_size、close_size、font_family、title_font_weight，另有 shadow 与 z_index。视觉只由 Theme 控制；holder 不占正文布局。默认 recipe 对照本地锁定 Ant Design 6.6.5（4a39f54842eade4e565ab336ef6097cd7e723cdd），不引入其运行时依赖。

Windows MSVC HEADLESS Debug/Release 的本次两组定向回归各合计 99/99；hidden SDL/D3D12 的 Message、Notification Loading 序列各通过 16 项检查，并对实际图标局部像素断言旋转。详见 [动画收尾结果](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-motion-windows.md)。历史各 182 项与 1,520 张图单独保留，不计入本次数量；Linux/system IME/AT 边界保持独立。

## Gallery

Message 保留原“处理提示”操作台，并新增带进度消息和非阻塞读取关闭结果。Notification 提供自己的 `gallery.sample.notification.center` 独立页面，单独挂载 NotificationHolder；可编辑审核备注、仅更新标题、切换四角与状态、添加限时通知体验折叠栈、关闭或清空。原 Message 页面仍保留两者组合示例作为交叉入口。

示例使用 `duration(0)`，让提示保留到显式关闭；自动到期继续由已有 notice 时钟专项合同验证。同 key 的局部更新不传新的 rich slots；显式关闭后的重开创建新的通知，最新业务备注作为新初值。091 的 `--overlays-timeline-only` 检查目录、真实操作和有限视口，进展见 [091 tasks](../openspec/changes/091-20261026-add-overlay-and-timeline-gallery/tasks.md)。
